#include "Combat/BreakerMonsterChassis.h"

#include "Items/BreakerAffixLibrary.h"

int32 UBreakerMonsterChassisLibrary::ClampAreaLevel(int32 AreaLevel)
{
    return FMath::Clamp(AreaLevel, BreakerMinAreaLevel, BreakerMaxAreaLevel);
}

float UBreakerMonsterChassisLibrary::GetChassisHealth(int32 AreaLevel, const FBreakerMonsterChassisParams& Params)
{
    const int32 Level = ClampAreaLevel(AreaLevel);
    const float Growth = FMath::Max(Params.HealthGrowthPerLevel, 0.0f);
    return FMath::Max(Params.BaseHealth, 0.0f) * FMath::Pow(1.0f + Growth, static_cast<float>(Level - 1));
}

float UBreakerMonsterChassisLibrary::GetChassisDamage(int32 AreaLevel, const FBreakerMonsterChassisParams& Params)
{
    const int32 Level = ClampAreaLevel(AreaLevel);
    const float Growth = FMath::Max(Params.DamageGrowthPerLevel, 0.0f);
    return FMath::Max(Params.BaseDamage, 0.0f) * FMath::Pow(1.0f + Growth, static_cast<float>(Level - 1));
}

float UBreakerMonsterChassisLibrary::GetRankHealthMultiplier(EBreakerMonsterRank Rank, const FBreakerMonsterChassisParams& Params)
{
    switch (Rank)
    {
    case EBreakerMonsterRank::Elite:           return FMath::Max(Params.EliteHealthMultiplier, 0.0f);
    case EBreakerMonsterRank::ModifierBearing: return FMath::Max(Params.ModifierHealthMultiplier, 0.0f);
    case EBreakerMonsterRank::Boss:            return FMath::Max(Params.BossHealthMultiplier, 0.0f);
    case EBreakerMonsterRank::Trash:
    default:                                   return 1.0f;
    }
}

float UBreakerMonsterChassisLibrary::GetRankDamageMultiplier(EBreakerMonsterRank Rank, const FBreakerMonsterChassisParams& Params)
{
    switch (Rank)
    {
    case EBreakerMonsterRank::Elite:           return FMath::Max(Params.EliteDamageMultiplier, 0.0f);
    case EBreakerMonsterRank::ModifierBearing: return FMath::Max(Params.ModifierDamageMultiplier, 0.0f);
    case EBreakerMonsterRank::Boss:            return FMath::Max(Params.BossDamageMultiplier, 0.0f);
    case EBreakerMonsterRank::Trash:
    default:                                   return 1.0f;
    }
}

float UBreakerMonsterChassisLibrary::GetMonsterHealth(int32 AreaLevel, EBreakerMonsterRank Rank,
    const FBreakerMonsterChassisParams& Params, float ArchetypeMultiplier)
{
    return GetChassisHealth(AreaLevel, Params)
        * GetRankHealthMultiplier(Rank, Params)
        * FMath::Max(ArchetypeMultiplier, 0.0f);
}

float UBreakerMonsterChassisLibrary::GetMonsterDamage(int32 AreaLevel, EBreakerMonsterRank Rank,
    const FBreakerMonsterChassisParams& Params, float ArchetypeMultiplier)
{
    return GetChassisDamage(AreaLevel, Params)
        * GetRankDamageMultiplier(Rank, Params)
        * FMath::Max(ArchetypeMultiplier, 0.0f);
}

int32 UBreakerMonsterChassisLibrary::GetDropItemLevel(int32 AreaLevel)
{
    // THIS WAS THE LAST LINK IN THE ENDGAME GAP, and it is now the identity.
    //
    // It used to clamp to 50 because the affix tier tables were authored only
    // that far, so a level-80 area's drops would have rolled off the end of the
    // curve. O29 removed that reason: the ladder runs to item level 120, past
    // both the character cap and the area-level ceiling.
    //
    // With the clamp gone this returns the area level unchanged across the
    // whole range, and THAT is what makes the two curves cancel. Monster health
    // grows at (1+g)^(AL-1) and weapon base damage at (1+w)^(ilvl-1) with
    // w = g, so ilvl = AL is exactly the condition under which a baseline
    // build holds a constant TTK from area level 1 to 100. While this clamped
    // at 50, the monster curve ran for fifty more levels than the player's and
    // the measured swing was 74x with nothing to answer it.
    //
    // ClampAreaLevel already bounds to 1..100, so the extra clamp is a
    // belt-and-braces guard against a caller that bypasses it, not a design
    // limit.
    return FMath::Clamp(ClampAreaLevel(AreaLevel), 1, UBreakerAffixLibrary::MaxItemLevel);
}

float UBreakerMonsterChassisLibrary::GetTimeToKillSeconds(float Health, float DamagePerSecond)
{
    if (DamagePerSecond <= KINDA_SMALL_NUMBER) return TNumericLimits<float>::Max();
    return FMath::Max(Health, 0.0f) / DamagePerSecond;
}
