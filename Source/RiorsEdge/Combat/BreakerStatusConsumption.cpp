#include "Combat/BreakerStatusConsumption.h"

float UBreakerStatusConsumption::DetonationDamage(int32 DistinctCount, const FBreakerDetonationParams& Params, EBreakerDetonationCurve Curve)
{
    const int32 Counted = FMath::Clamp(DistinctCount, 0, FMath::Max(1, Params.MaximumCountedStatuses));
    if (Counted <= 0) return 0.0f;

    // MS9 USES ITS OWN PER-STATUS CONSTANT. This read DamagePerDistinctStatus
    // for both curves and added the threshold bonus on top, so FixedPlusThreshold
    // was the base curve plus a constant -- strictly steeper than the curve it
    // is named for flattening. The reshape is the swapped term; the bonus alone
    // was never one.
    const bool bFixed = Curve == EBreakerDetonationCurve::FixedPlusThreshold;
    const float PerStatus = bFixed ? Params.FixedDamagePerDistinctStatus : Params.DamagePerDistinctStatus;

    float Damage = Params.FlatDamageIfAny + PerStatus * static_cast<float>(Counted);
    if (bFixed && Counted >= FMath::Max(1, Params.ThresholdCount))
    {
        Damage += Params.ThresholdFlatBonus;
    }
    return FMath::Max(0.0f, Damage);
}

float UBreakerStatusConsumption::DetonationRatio(int32 LowCount, int32 HighCount, const FBreakerDetonationParams& Params, EBreakerDetonationCurve Curve)
{
    const float Low = DetonationDamage(LowCount, Params, Curve);
    if (Low <= 0.0f) return 0.0f;
    return DetonationDamage(HighCount, Params, Curve) / Low;
}

float UBreakerStatusConsumption::RefundForConsumed(int32 DistinctCount, float RefundPerStatus, int32 MaximumCountedStatuses)
{
    const int32 Counted = FMath::Clamp(DistinctCount, 0, FMath::Max(1, MaximumCountedStatuses));
    return FMath::Max(0.0f, RefundPerStatus) * static_cast<float>(Counted);
}
