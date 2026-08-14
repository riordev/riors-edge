#include "Combat/BreakerMonsterChassis.h"

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
    // Affix tier tables are authored to the character cap of 50; the chassis
    // curve is not. Clamping here keeps a level-80 area's drops legal instead
    // of rolling off the end of the tier curve.
    return FMath::Clamp(ClampAreaLevel(AreaLevel), 1, 50);
}

float UBreakerMonsterChassisLibrary::GetTimeToKillSeconds(float Health, float DamagePerSecond)
{
    if (DamagePerSecond <= KINDA_SMALL_NUMBER) return TNumericLimits<float>::Max();
    return FMath::Max(Health, 0.0f) / DamagePerSecond;
}
