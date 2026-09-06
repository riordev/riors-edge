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

bool UBreakerBossPhaseLibrary::IsApparatusExposed(EBreakerBossPhase Phase, bool bOrderRaiseActive)
{
    if (Phase == EBreakerBossPhase::Commitment) return true;
    return bOrderRaiseActive;
}

bool UBreakerBossPhaseLibrary::IsPunishWindowOpen(EBreakerBossPhase Phase, bool bOrderRaiseActive, float FrontBreakWindowRemaining)
{
    // Three ways in, and any one holds it open. The front-break window is the
    // one that survives a gate: it is earned, and a phase change is not a
    // reason to take it back.
    return IsApparatusExposed(Phase, bOrderRaiseActive) || FrontBreakWindowRemaining > 0.0f;
}

float UBreakerBossPhaseLibrary::NextGate(EBreakerBossPhase Phase, const FBreakerBossPhaseParams& Params)
{
    // Normalised the same way GetPhaseForHealthFraction reads them, so the
    // gate this reports is the gate that phase actually crosses.
    const float High = FMath::Clamp(FMath::Max(Params.SuppressionGate, Params.CommitmentGate), 0.0f, 1.0f);
    const float Low = FMath::Clamp(FMath::Min(Params.SuppressionGate, Params.CommitmentGate), 0.0f, 1.0f);
    switch (Phase)
    {
    case EBreakerBossPhase::Deployment:  return High;
    case EBreakerBossPhase::Suppression: return Low;
    case EBreakerBossPhase::Commitment:
    default:                             return -1.0f;   // ends only with the boss
    }
}

bool UBreakerBossPhaseLibrary::AdvanceBreakWindow(float& Remaining, float DeltaSeconds)
{
    if (Remaining <= 0.0f)
    {
        Remaining = 0.0f;
        return false;
    }
    Remaining = FMath::Max(0.0f, Remaining - FMath::Max(0.0f, DeltaSeconds));
    // True only on the step that spends the last of it. A closed window does
    // not re-close on the next frame, so the actor's close runs exactly once.
    return Remaining <= 0.0f;
}

FBreakerBossGrammar UBreakerBossPhaseLibrary::MakeShippedGrammar(const FBreakerBossPhaseParams& Params,
    int32 AddsPerDeploy, int32 GalleryLatticeCount, float SweepTellSeconds)
{
    auto Beat = [](EBreakerBossBeat Kind, float Seconds, const TCHAR* Tag, float Gate = -1.0f, int32 Adds = 0)
    {
        FBreakerBossBeat Out;
        Out.Beat = Kind;
        Out.Seconds = Seconds;
        Out.GateFraction = Gate;
        Out.AddCount = Adds;
        Out.Tag = FName(Tag);
        return Out;
    };

    FBreakerBossGrammar Grammar;

    // Fight-level: the sweep draw-back is the tell the player trades against
    // at the front, and spending the front pool is the window it opens (O198).
    // The window lands once, in whichever phase the pool runs out.
    Grammar.FightLevel.Add(Beat(EBreakerBossBeat::Telegraph, FMath::Max(0.0f, SweepTellSeconds), TEXT("SweepDrawBack")));
    Grammar.FightLevel.Add(Beat(EBreakerBossBeat::PunishWindow, FMath::Max(0.0f, Params.FrontBreakPunishSeconds), TEXT("FrontBreak")));

    // Deployment: the raise IS both the tell and the window; the adds come
    // after it, previewed; the gate ends it.
    TArray<FBreakerBossBeat>& Deployment = Grammar.PerPhase[static_cast<int32>(EBreakerBossPhase::Deployment)];
    const float DeployRaise = GetOrderRaiseSeconds(EBreakerBossPhase::Deployment, Params);
    Deployment.Add(Beat(EBreakerBossBeat::Telegraph, DeployRaise, TEXT("DeployRaise")));
    Deployment.Add(Beat(EBreakerBossBeat::PunishWindow, DeployRaise, TEXT("DeployRaise")));
    Deployment.Add(Beat(EBreakerBossBeat::AddWave, FMath::Max(0.0f, Params.DeploySpawnDelaySeconds), TEXT("Deploy"),
        -1.0f, FMath::Max(0, AddsPerDeploy)));
    Deployment.Add(Beat(EBreakerBossBeat::PhaseGate, 0.0f, TEXT("SuppressionGate"),
        NextGate(EBreakerBossPhase::Deployment, Params)));

    // Suppression: the galleries fill on entry, then the FIRE raise and its
    // window, then the gate.
    TArray<FBreakerBossBeat>& Suppression = Grammar.PerPhase[static_cast<int32>(EBreakerBossPhase::Suppression)];
    const float FireRaise = GetOrderRaiseSeconds(EBreakerBossPhase::Suppression, Params);
    Suppression.Add(Beat(EBreakerBossBeat::AddWave, 0.0f, TEXT("GalleryLattices"),
        -1.0f, FMath::Clamp(GalleryLatticeCount, 0, 3)));
    Suppression.Add(Beat(EBreakerBossBeat::Telegraph, FireRaise, TEXT("FireRaise")));
    Suppression.Add(Beat(EBreakerBossBeat::PunishWindow, FireRaise, TEXT("FireRaise")));
    Suppression.Add(Beat(EBreakerBossBeat::PhaseGate, 0.0f, TEXT("CommitmentGate"),
        NextGate(EBreakerBossPhase::Suppression, Params)));

    // Commitment: the window never closes, and the floor starts running out.
    TArray<FBreakerBossBeat>& Commitment = Grammar.PerPhase[static_cast<int32>(EBreakerBossPhase::Commitment)];
    Commitment.Add(Beat(EBreakerBossBeat::PunishWindow, -1.0f, TEXT("Commitment")));
    Commitment.Add(Beat(EBreakerBossBeat::ArenaChange, 0.0f, TEXT("SlamHazard")));

    return Grammar;
}

const FBreakerBossBeat* UBreakerBossPhaseLibrary::FirstPunishWindow(const FBreakerBossGrammar& Grammar)
{
    for (const FBreakerBossBeat& B : Grammar.FightLevel)
    {
        if (B.Beat == EBreakerBossBeat::PunishWindow) return &B;
    }
    for (const TArray<FBreakerBossBeat>& List : Grammar.PerPhase)
    {
        for (const FBreakerBossBeat& B : List)
        {
            if (B.Beat == EBreakerBossBeat::PunishWindow) return &B;
        }
    }
    return nullptr;
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
