#include "Combat/BreakerBossPhases.h"

EBreakerBossPhase UBreakerBossPhaseLibrary::GetPhaseForHealthFraction(float HealthFraction, const FBreakerBossPhaseParams& Params)
{
    // Gates are normalised rather than trusted: a params block with the
    // commitment gate above the suppression gate would otherwise skip a phase
    // silently, and a skipped phase is a fight the player never sees.
    const float High = FMath::Max(Params.SuppressionGate, Params.CommitmentGate);
    const float Low = FMath::Min(Params.SuppressionGate, Params.CommitmentGate);
    const float Fraction = FMath::Clamp(HealthFraction, 0.0f, 1.0f);

    if (Fraction > High) return EBreakerBossPhase::Deployment;
    if (Fraction > Low) return EBreakerBossPhase::Suppression;
    return EBreakerBossPhase::Commitment;
}

EBreakerBossPhase UBreakerBossPhaseLibrary::AdvancePhase(EBreakerBossPhase Current, float HealthFraction, const FBreakerBossPhaseParams& Params)
{
    const EBreakerBossPhase Implied = GetPhaseForHealthFraction(HealthFraction, Params);
    // MONOTONIC. A boss that is healed, that gains a shield, or whose max
    // health is rebuilt underneath it (ApplyChassis runs on any area-level
    // change) must not walk back into a phase it already left. Without this,
    // the fight has no defined length: a single heal re-runs the DEPLOY script
    // and the add count grows without bound.
    return static_cast<uint8>(Implied) > static_cast<uint8>(Current) ? Implied : Current;
}

EBreakerBossOrder UBreakerBossPhaseLibrary::GetOrderForPhase(EBreakerBossPhase Phase)
{
    switch (Phase)
    {
    case EBreakerBossPhase::Deployment:  return EBreakerBossOrder::Deploy;
    case EBreakerBossPhase::Suppression: return EBreakerBossOrder::Fire;
    case EBreakerBossPhase::Commitment:
    default:                             return EBreakerBossOrder::None;
    }
}

float UBreakerBossPhaseLibrary::GetOrderIntervalSeconds(EBreakerBossPhase Phase, const FBreakerBossPhaseParams& Params)
{
    switch (Phase)
    {
    case EBreakerBossPhase::Deployment:  return FMath::Max(0.1f, Params.DeployIntervalSeconds);
    case EBreakerBossPhase::Suppression: return FMath::Max(0.1f, Params.FireIntervalSeconds);
    case EBreakerBossPhase::Commitment:
    default:                             return -1.0f;   // it has stopped commanding
    }
}

float UBreakerBossPhaseLibrary::GetOrderRaiseSeconds(EBreakerBossPhase Phase, const FBreakerBossPhaseParams& Params)
{
    switch (Phase)
    {
    case EBreakerBossPhase::Deployment:  return FMath::Max(0.0f, Params.DeployRaiseSeconds);
    case EBreakerBossPhase::Suppression: return FMath::Max(0.0f, Params.FireRaiseSeconds);
    case EBreakerBossPhase::Commitment:
    default:                             return 0.0f;
    }
}

bool UBreakerBossPhaseLibrary::AdvanceOrderClock(float& TimeSinceLastOrder, float DeltaSeconds, float IntervalSeconds)
{
    // A negative interval is "no orders in this phase" and must not be a
    // divide-by-anything or a clock that silently keeps counting.
    if (IntervalSeconds <= 0.0f)
    {
        TimeSinceLastOrder = 0.0f;
        return false;
    }
    TimeSinceLastOrder += FMath::Max(0.0f, DeltaSeconds);
    if (TimeSinceLastOrder < IntervalSeconds) return false;
    // RESET, never subtract. Subtracting would let a 60-second hitch bank three
    // orders and deploy six Skitters on one frame, which §5.1 forbids outright
    // (spawns are always previewed) and which reads as a bug rather than as
    // difficulty.
    TimeSinceLastOrder = 0.0f;
    return true;
}

float UBreakerBossPhaseLibrary::GetPhaseSpeedMultiplier(EBreakerBossPhase Phase, const FBreakerBossPhaseParams& Params)
{
    return Phase == EBreakerBossPhase::Commitment
        ? FMath::Max(1.0f, Params.CommitmentSpeedMultiplier) : 1.0f;
}

float UBreakerBossPhaseLibrary::GetPhaseSweepCooldown(EBreakerBossPhase Phase, float BaseCooldown, const FBreakerBossPhaseParams& Params)
{
    return Phase == EBreakerBossPhase::Commitment
        ? BaseCooldown * FMath::Clamp(Params.CommitmentSweepCadenceScale, 0.05f, 1.0f) : BaseCooldown;
}

float UBreakerBossPhaseLibrary::GetPhaseSlamCooldown(EBreakerBossPhase Phase, float BaseCooldown, const FBreakerBossPhaseParams& Params)
{
    return Phase == EBreakerBossPhase::Commitment
        ? FMath::Max(0.0f, Params.CommitmentSlamCooldownSeconds) : BaseCooldown;
}

float UBreakerBossPhaseLibrary::GetPhaseFrontalArmor(EBreakerBossPhase Phase, float BaseArmor, const FBreakerBossPhaseParams& Params)
{
    // "Its damage output rises and its defence falls" (§3.4). The trade is the
    // whole reason phase 3 is survivable at all: the boss stops being a wall.
    return Phase == EBreakerBossPhase::Commitment
        ? BaseArmor * FMath::Clamp(Params.CommitmentArmorFraction, 0.0f, 1.0f) : BaseArmor;
}

bool UBreakerBossPhaseLibrary::IsApparatusExposed(EBreakerBossPhase Phase, bool bOrderRaiseActive)
{
    if (Phase == EBreakerBossPhase::Commitment) return true;
    return bOrderRaiseActive;
}

bool UBreakerBossPhaseLibrary::ShouldSpawnAdds(EBreakerBossPhase Phase)
{
    return Phase != EBreakerBossPhase::Commitment;
}

FString UBreakerBossPhaseLibrary::GetPhaseName(EBreakerBossPhase Phase)
{
    switch (Phase)
    {
    case EBreakerBossPhase::Deployment:  return TEXT("DEPLOYMENT");
    case EBreakerBossPhase::Suppression: return TEXT("SUPPRESSION");
    case EBreakerBossPhase::Commitment:  return TEXT("COMMITMENT");
    default:                             return FString();
    }
}

FString UBreakerBossPhaseLibrary::GetOrderName(EBreakerBossOrder Order)
{
    switch (Order)
    {
    case EBreakerBossOrder::Deploy: return TEXT("DEPLOY");
    case EBreakerBossOrder::Fire:   return TEXT("FIRE");
    case EBreakerBossOrder::None:
    default:                        return FString();
    }
}
