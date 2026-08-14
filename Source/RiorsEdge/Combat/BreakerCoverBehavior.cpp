#include "Combat/BreakerCoverBehavior.h"

namespace BreakerCoverDetail
{
    // Distinctively prefixed for the unity build.
    static constexpr float BreakerRejectedCoverScore = TNumericLimits<float>::Max();
}

float UBreakerCoverLibrary::GetRejectedCoverScore()
{
    return BreakerCoverDetail::BreakerRejectedCoverScore;
}

TArray<FVector> UBreakerCoverLibrary::GenerateCoverCandidates(const FVector& Origin, const FBreakerCoverParams& Params, int32 Seed)
{
    TArray<FVector> Candidates;
    const int32 Count = FMath::Max(1, Params.CandidateCount);
    const float Radius = FMath::Max(0.0f, Params.SearchRadiusCm);
    Candidates.Reserve(Count);

    FRandomStream Stream(Seed);
    // A random phase offset, so two skirmishers standing in the same place do
    // not evaluate identical rings and break in identical directions. Without
    // it a pack of them moves as one object, which is exactly the "conveyor
    // belt" read the three-gear chase was built to kill.
    const float PhaseOffset = Stream.FRandRange(0.0f, 360.0f);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const float Angle = PhaseOffset + 360.0f / Count * Index;
        // Two radii per direction — a short break and a long one — so the enemy
        // has a nearby option behind the crate it is next to as well as a
        // committed one behind the wall.
        const float Scale = (Index % 2 == 0) ? 0.55f : 1.0f;
        Candidates.Add(Origin + FRotator(0.0f, Angle, 0.0f).RotateVector(FVector(Radius * Scale, 0.0f, 0.0f)));
    }
    return Candidates;
}

float UBreakerCoverLibrary::ScoreCoverCandidate(const FVector& Candidate, const FVector& CurrentLocation,
    const FVector& ThreatLocation, const FBreakerCoverParams& Params)
{
    const float Min = FMath::Min(Params.PreferredMinRangeCm, Params.PreferredMaxRangeCm);
    const float Max = FMath::Max(Params.PreferredMinRangeCm, Params.PreferredMaxRangeCm);
    const float Range = FVector::Dist2D(Candidate, ThreatLocation);
    // Outside the band is a rejection and not a penalty. A cover point three
    // metres from the player is not cover, and one forty metres away is a
    // different fight — neither is worth scoring against the other.
    if (Range < Min || Range > Max) return BreakerCoverDetail::BreakerRejectedCoverScore;

    const float Travel = FVector::Dist2D(Candidate, CurrentLocation);
    // Distance from the middle of the band, so a point at either edge is worse
    // than one in the centre by the same amount.
    const float RangeError = FMath::Abs(Range - (Min + Max) * 0.5f);
    return Travel * FMath::Max(0.0f, Params.TravelCostWeight)
         + RangeError * FMath::Max(0.0f, Params.RangeCostWeight);
}

bool UBreakerCoverLibrary::ChooseCoverPoint(const TArray<FVector>& BlockedCandidates, const FVector& CurrentLocation,
    const FVector& ThreatLocation, const FBreakerCoverParams& Params, FVector& OutPoint)
{
    float BestScore = BreakerCoverDetail::BreakerRejectedCoverScore;
    bool bFound = false;
    for (const FVector& Candidate : BlockedCandidates)
    {
        const float Score = ScoreCoverCandidate(Candidate, CurrentLocation, ThreatLocation, Params);
        if (Score >= BreakerCoverDetail::BreakerRejectedCoverScore) continue;
        if (bFound && Score >= BestScore) continue;
        BestScore = Score;
        OutPoint = Candidate;
        bFound = true;
    }
    return bFound;
}

FString UBreakerCoverLibrary::GetCoverStateName(EBreakerCoverState State)
{
    switch (State)
    {
    case EBreakerCoverState::Relocating: return TEXT("BREAKING");
    case EBreakerCoverState::InCover:    return TEXT("IN COVER");
    case EBreakerCoverState::Exposed:    return TEXT("FIRING");
    case EBreakerCoverState::Flinched:   return TEXT("FLINCH");
    default:                             return FString();
    }
}
