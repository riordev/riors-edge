#include "Combat/BreakerStatusConsumption.h"

float UBreakerStatusConsumption::DetonationDamage(int32 DistinctCount, const FBreakerDetonationParams& Params, EBreakerDetonationCurve Curve)
{
    const int32 Counted = FMath::Clamp(DistinctCount, 0, FMath::Max(1, Params.MaximumCountedStatuses));
    if (Counted <= 0) return 0.0f;

    float Damage = Params.FlatDamageIfAny + Params.DamagePerDistinctStatus * static_cast<float>(Counted);
    if (Curve == EBreakerDetonationCurve::FixedPlusThreshold && Counted >= FMath::Max(1, Params.ThresholdCount))
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
