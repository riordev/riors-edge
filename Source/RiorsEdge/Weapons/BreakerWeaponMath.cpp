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
