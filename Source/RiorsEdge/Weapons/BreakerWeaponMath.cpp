#include "Weapons/BreakerWeaponMath.h"
#include "Weapons/BreakerWeaponDefinition.h"

float FBreakerWeaponMath::FireInterval(float RoundsPerMinute)
{
    return 60.0f / FMath::Max(1.0f, RoundsPerMinute);
}

float FBreakerWeaponMath::DamageMultiplierAtDistance(const UBreakerWeaponDefinition* Definition, float Distance)
{
    if (!Definition || Distance <= Definition->FalloffStart) return 1.0f;
    if (Distance >= Definition->FalloffEnd) return Definition ? Definition->MinimumFalloffMultiplier : 1.0f;
    const float Alpha = FMath::GetRangePct(Definition->FalloffStart, Definition->FalloffEnd, Distance);
    return FMath::Lerp(1.0f, Definition->MinimumFalloffMultiplier, Alpha);
}

FVector FBreakerWeaponMath::ApplyConeSpread(const FVector& Direction, float SpreadDegrees, int32 RandomSeed)
{
    if (SpreadDegrees <= 0.0f) return Direction.GetSafeNormal();
    FRandomStream Random(RandomSeed);
    return Random.VRandCone(Direction.GetSafeNormal(), FMath::DegreesToRadians(SpreadDegrees));
}

float FBreakerWeaponMath::DistanceFromRayToPoint(const FVector& RayOrigin, const FVector& RayDirection, const FVector& Point)
{
    const FVector Direction = RayDirection.GetSafeNormal();
    if (Direction.IsNearlyZero()) return static_cast<float>(FVector::Dist(RayOrigin, Point));
    const FVector ToPoint = Point - RayOrigin;
    // Clamped at zero: only what is downrange of the muzzle can be shot.
    const double Along = FMath::Max(0.0, FVector::DotProduct(ToPoint, Direction));
    return static_cast<float>(FVector::Dist(RayOrigin + Direction * Along, Point));
}

bool FBreakerWeaponMath::IsWithinWeakPointTolerance(const FVector& RayOrigin, const FVector& RayDirection, const FVector& WeakPointCenter, float WeakPointRadius, float ToleranceCm)
{
    const float Accept = FMath::Max(0.0f, WeakPointRadius) + FMath::Max(0.0f, ToleranceCm);
    if (Accept <= 0.0f) return false;
    return DistanceFromRayToPoint(RayOrigin, RayDirection, WeakPointCenter) <= Accept;
}

float FBreakerWeaponMath::ItemLevelDamageScalar(int32 ItemLevel, float GrowthPerLevel)
{
    const int32 Level = FMath::Clamp(ItemLevel, 1, MaxSupportedItemLevel);
    // A growth of 0 (or a nonsense negative) means "item level does nothing",
    // which is the pre-curve behaviour and a legitimate A/B setting.
    const float Growth = FMath::Max(0.0f, GrowthPerLevel);
    if (Growth <= 0.0f || Level <= 1) return 1.0f;
    return FMath::Pow(1.0f + Growth, static_cast<float>(Level - 1));
}

float FBreakerWeaponMath::WeaponBaseDamage(float ArchetypeBase, int32 ItemLevel, float GrowthPerLevel)
{
    return FMath::Max(0.0f, ArchetypeBase) * ItemLevelDamageScalar(ItemLevel, GrowthPerLevel);
}

int32 FBreakerWeaponMath::ConsumeMultishot(float AdditionalProjectiles, float& Accumulator)
{
    if (AdditionalProjectiles <= 0.0f) return 0;
    // Bank the whole request, then pay out the whole pellets. The accumulator
    // keeps only the sub-pellet remainder, so it can never grow unboundedly
    // and a channel dropping back to zero leaves at most a fraction stranded.
    Accumulator += AdditionalProjectiles;
    const int32 WholePellets = FMath::FloorToInt32(Accumulator);
    Accumulator -= static_cast<float>(WholePellets);
    return WholePellets;
}

float FBreakerWeaponMath::NextPierceMultiplier(float CurrentMultiplier, float FalloffPerTarget, bool bPreviousHitKilled, bool bOverpenetration)
{
    // M10: "a shot that kills carries on at full damage instead of falling
    // off" — the falloff STEP is skipped, not the whole ladder reset.
    if (bOverpenetration && bPreviousHitKilled) return CurrentMultiplier;
    return CurrentMultiplier * FMath::Clamp(FalloffPerTarget, 0.0f, 1.0f);
}

int32 FBreakerWeaponMath::SelectNearestTarget(const FVector& Origin, const TArray<FVector>& Candidates, float MaxRadiusCm)
{
    int32 BestIndex = INDEX_NONE;
    double BestDistanceSquared = FMath::Square(static_cast<double>(FMath::Max(0.0f, MaxRadiusCm)));
    for (int32 Index = 0; Index < Candidates.Num(); ++Index)
    {
        const double DistanceSquared = FVector::DistSquared(Origin, Candidates[Index]);
        // Strictly-less keeps the tie deterministic at the lower index.
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestIndex = Index;
        }
    }
    return BestIndex;
}

int32 FBreakerWeaponMath::SecondaryShotSeed(uint32 OwnerHash, int32 ShotSequence, uint32 Salt, int32 Index)
{
    // Same seed material as the primary draws, salted twice: once by the
    // channel (multishot vs chain vs ricochet) and once by the index within
    // it, so no sub-stream can collide with the primary sequence or with a
    // sibling.
    return static_cast<int32>(HashCombine(HashCombine(OwnerHash, static_cast<uint32>(ShotSequence)), Salt + static_cast<uint32>(Index)));
}
