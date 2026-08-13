#include "Combat/BreakerRangedBehavior.h"

EBreakerRangedBand UBreakerRangedBehaviorLibrary::ClassifyBand(float Distance, float MinDistance, float MaxDistance,
    float Hysteresis, EBreakerRangedBand PreviousBand)
{
    // Tolerate a mis-authored band rather than producing nonsense.
    float Low = FMath::Min(MinDistance, MaxDistance);
    float High = FMath::Max(MinDistance, MaxDistance);
    Low = FMath::Max(Low, 0.0f);
    High = FMath::Max(High, Low);

    // A hysteresis wider than half the band would let the widened Hold region
    // swallow both edges and the enemy would never leave it.
    const float MaxUsable = FMath::Max((High - Low) * 0.5f, 0.0f);
    const float H = FMath::Clamp(Hysteresis, 0.0f, MaxUsable);

    float LowEdge = Low;
    float HighEdge = High;
    switch (PreviousBand)
    {
    case EBreakerRangedBand::Hold:
        // Already holding: tolerate drifting slightly outside before reacting.
        LowEdge = Low - H;
        HighEdge = High + H;
        break;
    case EBreakerRangedBand::Advance:
        // Committed to closing: keep closing until meaningfully inside.
        HighEdge = High - H;
        break;
    case EBreakerRangedBand::Retreat:
        // Committed to backing off: keep going until meaningfully clear.
        LowEdge = Low + H;
        break;
    default:
        break;
    }

    if (Distance > HighEdge) return EBreakerRangedBand::Advance;
    if (Distance < LowEdge) return EBreakerRangedBand::Retreat;
    return EBreakerRangedBand::Hold;
}

float UBreakerRangedBehaviorLibrary::GetBandRadialSign(EBreakerRangedBand Band)
{
    switch (Band)
    {
    case EBreakerRangedBand::Advance: return 1.0f;
    case EBreakerRangedBand::Retreat: return -1.0f;
    default: return 0.0f;
    }
}

float UBreakerRangedBehaviorLibrary::GetBandSpeedScale(EBreakerRangedBand Band, float AdvanceScale, float RetreatScale, float StrafeScale)
{
    switch (Band)
    {
    case EBreakerRangedBand::Advance: return AdvanceScale;
    case EBreakerRangedBand::Retreat: return RetreatScale;
    default: return StrafeScale;
    }
}

float UBreakerRangedBehaviorLibrary::StepEngagementDistance(float Distance, float MinDistance, float MaxDistance, float Hysteresis,
    float MoveSpeed, float AdvanceScale, float RetreatScale, float DeltaSeconds, EBreakerRangedBand& InOutBand)
{
    InOutBand = ClassifyBand(Distance, MinDistance, MaxDistance, Hysteresis, InOutBand);
    // Strafing is lateral, so it contributes nothing radially — 0 here is the
    // one-dimensional shadow of "hold station and circle".
    const float Scale = GetBandSpeedScale(InOutBand, AdvanceScale, RetreatScale, 0.0f);
    const float Sign = GetBandRadialSign(InOutBand);
    // Sign is +1 when closing, which REDUCES the distance to the player.
    return FMath::Max(Distance - Sign * MoveSpeed * Scale * DeltaSeconds, 0.0f);
}

bool UBreakerRangedBehaviorLibrary::SolveInterceptTime(const FVector& ShooterLocation, const FVector& TargetLocation,
    const FVector& TargetVelocity, float ProjectileSpeed, float& OutTime)
{
    OutTime = 0.0f;
    if (ProjectileSpeed <= KINDA_SMALL_NUMBER) return false;

    // |P + V t| = S t  ->  (V.V - S^2) t^2 + 2 (P.V) t + P.P = 0
    const FVector P = TargetLocation - ShooterLocation;
    const FVector& V = TargetVelocity;
    const double A = V.SizeSquared() - static_cast<double>(ProjectileSpeed) * ProjectileSpeed;
    const double B = 2.0 * FVector::DotProduct(P, V);
    const double C = P.SizeSquared();

    if (C <= UE_DOUBLE_KINDA_SMALL_NUMBER)
    {
        // Already on top of the target.
        return true;
    }

    if (FMath::Abs(A) <= 1e-6)
    {
        // Target speed exactly equals projectile speed: the quadratic degrades
        // to linear.
        if (FMath::Abs(B) <= 1e-6) return false;
        const double Linear = -C / B;
        if (Linear <= 0.0) return false;
        OutTime = static_cast<float>(Linear);
        return true;
    }

    const double Discriminant = B * B - 4.0 * A * C;
    if (Discriminant < 0.0) return false;

    const double Root = FMath::Sqrt(Discriminant);
    const double T0 = (-B - Root) / (2.0 * A);
    const double T1 = (-B + Root) / (2.0 * A);

    // Smallest strictly positive root — the first time the shot can arrive.
    double Best = -1.0;
    if (T0 > 0.0) Best = T0;
    if (T1 > 0.0 && (Best < 0.0 || T1 < Best)) Best = T1;
    if (Best <= 0.0) return false;

    OutTime = static_cast<float>(Best);
    return true;
}

FVector UBreakerRangedBehaviorLibrary::ComputeAimPoint(const FVector& ShooterLocation, const FVector& TargetLocation,
    const FVector& TargetVelocity, float ProjectileSpeed, float LeadFraction)
{
    const float Fraction = FMath::Clamp(LeadFraction, 0.0f, 1.0f);
    if (Fraction <= 0.0f) return TargetLocation;

    float InterceptTime = 0.0f;
    if (!SolveInterceptTime(ShooterLocation, TargetLocation, TargetVelocity, ProjectileSpeed, InterceptTime))
    {
        // Unsolvable (the player is outrunning the shot): fire where they are.
        // Deliberately not "fire ahead anyway" — an unsolvable lead produces
        // wild aim that reads as a broken enemy rather than a missed shot.
        return TargetLocation;
    }
    return TargetLocation + TargetVelocity * InterceptTime * Fraction;
}

float UBreakerRangedBehaviorLibrary::GetTelegraphAlpha(float ElapsedSeconds, float WindupSeconds)
{
    if (WindupSeconds <= KINDA_SMALL_NUMBER) return 1.0f;
    return FMath::Clamp(ElapsedSeconds / WindupSeconds, 0.0f, 1.0f);
}
