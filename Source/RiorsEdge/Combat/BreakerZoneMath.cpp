#include "Combat/BreakerZoneMath.h"

bool UBreakerZoneMath::IsInsideZone(const FVector& ZoneCenter, float RadiusCm, float HalfHeightCm, const FVector& Point)
{
    if (RadiusCm <= 0.0f) return false;
    const FVector Offset = Point - ZoneCenter;
    if (HalfHeightCm > 0.0f)
    {
        if (FMath::Abs(Offset.Z) > HalfHeightCm) return false;
        return FVector2D(Offset.X, Offset.Y).SizeSquared() <= static_cast<double>(RadiusCm) * RadiusCm;
    }
    return Offset.SizeSquared() <= static_cast<double>(RadiusCm) * RadiusCm;
}

int32 UBreakerZoneMath::ConsumeTicks(float& TimeUntilNextTick, float DeltaSeconds, float TickInterval, int32 MaximumTicksPerAdvance)
{
    if (TickInterval <= 0.0f || DeltaSeconds <= 0.0f) return 0;

    TimeUntilNextTick -= DeltaSeconds;
    int32 Ticks = 0;
    const int32 Ceiling = FMath::Max(1, MaximumTicksPerAdvance);
    while (TimeUntilNextTick <= 0.0f && Ticks < Ceiling)
    {
        TimeUntilNextTick += TickInterval;
        ++Ticks;
    }
    // Hitch guard. Everything still owed is discarded rather than banked: a
    // zone must not fire a burst of damage the instant a stall ends.
    if (TimeUntilNextTick <= 0.0f) TimeUntilNextTick = TickInterval;
    return Ticks;
}

float UBreakerZoneMath::RemainingAfter(float Remaining, float DeltaSeconds, bool bPaused)
{
    if (bPaused) return Remaining;
    return Remaining - FMath::Max(0.0f, DeltaSeconds);
}

bool UBreakerZoneMath::ShouldRefreshExisting(const FVector& ExistingCenter, const FVector& NewCenter, float RadiusCm, float RefreshFractionOfRadius)
{
    if (RadiusCm <= 0.0f) return false;
    const float Threshold = RadiusCm * FMath::Max(0.0f, RefreshFractionOfRadius);
    return FVector::DistSquared(ExistingCenter, NewCenter) <= static_cast<double>(Threshold) * Threshold;
}
