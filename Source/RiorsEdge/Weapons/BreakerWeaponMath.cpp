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
