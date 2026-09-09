#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BreakerMeleeSweep.generated.h"

// Parameters for one melee arc. SB8 Edge widens ArcDegrees to 180; nothing
// else about the sweep changes, so the arc is a parameter rather than a branch.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerMeleeSweepParams
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FVector Origin = FVector::ZeroVector;
    // Expected normalized and horizontal. Cleave is a forward arc, not a cone
    // that can be aimed at the floor.
    UPROPERTY(BlueprintReadWrite) FVector Forward = FVector::ForwardVector;
    UPROPERTY(BlueprintReadWrite, meta=(ClampMin="0")) float RangeCm {}; // O246: authored in Data/abilities.json.
    // Full included angle, so 90 means 45 degrees either side of Forward.
    UPROPERTY(BlueprintReadWrite, meta=(ClampMin="0", ClampMax="360")) float ArcDegrees {}; // O246: authored in Data/abilities.json.
    // 0 means no limit.
    UPROPERTY(BlueprintReadWrite, meta=(ClampMin="0")) int32 MaxTargets = 0;
};

// Melee target selection for C1 Cleave (Ability-Implementation-Spec §5.1).
//
// DEVIATION FROM SPEC: the spec places this at Combat/BreakerMeleeLibrary.h.
// It lives under Abilities/ because Cleave is its only caller today and this
// agent does not own Combat/. Move it to Combat/ when a second melee source
// (Tank's Bulwark verbs) appears — the API is the spec's and the move is
// mechanical.
UCLASS()
class RIORSEDGE_API UBreakerMeleeSweep : public UObject
{
    GENERATED_BODY()

public:
    // Pure rule, world-free and testable: is a point inside the arc? The range
    // test is horizontal-plane only so an enemy standing on a crate is not
    // silently out of reach.
    //
    // GAP, recorded not faked: this reads CENTRE TO CENTRE. The overlap that
    // feeds it is a sphere against the target's collision, so a body already
    // touching the sphere is still rejected when its actor origin sits past
    // RangeCm — the swing lands short of what the player sees by both capsule
    // radii. Fixing it means the arc test taking a target radius, which is an
    // API change across every future melee source; the authored range carries
    // the difference until a second melee verb makes that change worth making.
    UFUNCTION(BlueprintPure, Category="Combat|Melee")
    static bool IsInsideArc(const FVector& Origin, const FVector& Forward, const FVector& TargetLocation, float RangeCm, float ArcDegrees);

    // Deterministic ordering: nearest-to-centre first, then nearest by
    // distance. Deterministic so a replay, a server, and a test all resolve the
    // same target list from the same geometry.
    UFUNCTION(BlueprintPure, Category="Combat|Melee")
    static float SortKey(const FVector& Origin, const FVector& Forward, const FVector& TargetLocation);

    // Server-only. Overlaps pawns in a sphere of Params.RangeCm, keeps those
    // inside the arc, drops the instigator, and returns them in SortKey order.
    static TArray<AActor*> SweepTargets(const UWorld* World, AActor* Instigator, const FBreakerMeleeSweepParams& Params);
};
