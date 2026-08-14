#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BreakerCoverBehavior.generated.h"

// Cover discipline, as pure world-free maths — the precedent is
// Combat/BreakerRangedBehavior.h, and the reason is the same: WHERE to stand is
// a decision, and a decision with an actor in it is a decision nobody can test.
// The one thing that genuinely needs a world — "is the line from the threat to
// this point blocked" — stays in the actor, and everything around it is here.
//
// This exists because Story-Source §1.5 makes cover use a readable marker of
// EARLY severance on an Altered: "early stage still wears insignia, still uses
// cover, still flinches." No enemy in the project did any of those; every one
// of them either closes to contact or holds a band, so every fight is answered
// by the same verb. An enemy that breaks line of sight and makes the player
// come to it is a different question.

// What a cover-using enemy is doing right now. The player reads this loop and
// learns to time the push, which is the whole counterplay: you cannot out-shoot
// something that is behind a wall, so you have to move.
UENUM(BlueprintType)
enum class EBreakerCoverState : uint8
{
    // Moving to a chosen cover point. Vulnerable, and deliberately so — this
    // is the window the player is being taught to punish.
    Relocating,
    // Behind cover, out of line of sight, waiting out the peek delay.
    InCover,
    // Stood up and firing. The exposure is the trade it makes for damage.
    Exposed,
    // Hit while exposed. Interrupted, and heading back to cover.
    Flinched
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerCoverParams
{
    GENERATED_BODY()

    // How far the enemy will look for cover. Small on purpose: an enemy that
    // sprints 30 m across the arena to a better wall reads as fleeing, not as
    // taking cover, and it removes the player's chance to push.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float SearchRadiusCm = 1400.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 CandidateCount = 16;   // O2 PLACEHOLDER
    // It wants to be in this band from the threat. Too close and its cover is
    // trivially flanked; too far and it stops being a threat at all.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float PreferredMinRangeCm = 700.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float PreferredMaxRangeCm = 2600.0f;   // O2 PLACEHOLDER
    // How strongly it prefers a nearby cover point over an ideally-ranged one.
    // High, because committing to a long relocation is what makes it look like
    // it is running away.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float TravelCostWeight = 1.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float RangeCostWeight = 0.35f;   // O2 PLACEHOLDER
};

UCLASS()
class RIORSEDGE_API UBreakerCoverLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // A ring of candidate standing positions around the enemy. Deterministic in
    // the seed so a pack does not all pick the same point and so a test can
    // reproduce a choice exactly.
    //
    // Points are generated on a ring rather than a grid because the enemy is
    // choosing "which way do I break", not "where in the room would I like to
    // be" — the second is pathfinding and this project has none.
    UFUNCTION(BlueprintPure, Category="Enemy|Cover")
    static TArray<FVector> GenerateCoverCandidates(const FVector& Origin, const FBreakerCoverParams& Params, int32 Seed);

    // Lower is better. Returns a large sentinel for a candidate outside the
    // preferred range band, so the caller does not need a second rejection
    // pass and cannot forget one.
    UFUNCTION(BlueprintPure, Category="Enemy|Cover")
    static float ScoreCoverCandidate(const FVector& Candidate, const FVector& CurrentLocation,
        const FVector& ThreatLocation, const FBreakerCoverParams& Params);

    // The sentinel a rejected candidate scores. Public so a caller can tell
    // "the best option is bad" from "there are no options".
    UFUNCTION(BlueprintPure, Category="Enemy|Cover")
    static float GetRejectedCoverScore();

    // Picks the best candidate from a list already filtered to the ones the
    // CALLER proved are actually blocked from the threat. Returns false when
    // the list is empty or every entry was rejected, which is the "no cover
    // here — fight in the open" case an archetype must handle.
    UFUNCTION(BlueprintPure, Category="Enemy|Cover")
    static bool ChooseCoverPoint(const TArray<FVector>& BlockedCandidates, const FVector& CurrentLocation,
        const FVector& ThreatLocation, const FBreakerCoverParams& Params, FVector& OutPoint);

    UFUNCTION(BlueprintPure, Category="Enemy|Cover")
    static FString GetCoverStateName(EBreakerCoverState State);
};
