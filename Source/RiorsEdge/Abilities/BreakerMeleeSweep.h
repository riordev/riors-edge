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
    // THE GAP THIS ONCE RECORDED IS CLOSED, and both conditions the note named
    // as its trigger have fired. It read CENTRE TO CENTRE while the overlap
    // that feeds it is a sphere against the target's collision, so a body
    // already touching the sphere was thrown away when its actor origin sat
    // past RangeCm. TargetRadiusCm is that forgiveness, and it defaults to zero
    // so a caller with no body — a pure test, a point of interest — keeps the
    // old rule exactly.
    //
    // The old note had the magnitude wrong and it is worth saying why, because
    // the correction is what makes the fix small. It is NOT "both capsule
    // radii": the sphere is centred on the swinger's own actor origin, so the
    // swinger's radius never enters the query at all, and the target's CAPSULE
    // is ECR_Ignore on the very channel the overlap runs on. What the sphere
    // actually touches is the damageable hit box. The wasted band was one box
    // half-extent, not two capsule radii.
    //
    // It also missed a second defect outright: the shortfall was YAW-DEPENDENT.
    // A box reaches its half-extent face-on and its diagonal corner-on, so the
    // same body at the same distance was hittable or not according to how it
    // happened to be standing. That is why the forgiveness below is INSCRIBED
    // rather than circumscribed — see HorizontalReach.
    UFUNCTION(BlueprintPure, Category="Combat|Melee")
    static bool IsInsideArc(const FVector& Origin, const FVector& Forward, const FVector& TargetLocation, float RangeCm, float ArcDegrees, float TargetRadiusCm = 0.0f);

    // The forgiveness a body earns, from the half-extents of the shape the
    // sweep's own overlap admitted it by.
    //
    // INSCRIBED — the SMALLER horizontal half-extent — and that is the whole
    // point. The circumscribed radius would forgive more, but only from some
    // angles, which keeps the yaw-dependence and merely moves it outward. The
    // inscribed radius is the largest forgiveness that is true from EVERY yaw,
    // so a body at a given distance is hittable or not for one reason only.
    UFUNCTION(BlueprintPure, Category="Combat|Melee")
    static float HorizontalReach(const FVector& BoxExtent)
    {
        return FMath::Max(0.0f, static_cast<float>(FMath::Min(BoxExtent.X, BoxExtent.Y)));
    }

    // Deterministic ordering: nearest-to-centre first, then nearest by
    // distance. Deterministic so a replay, a server, and a test all resolve the
    // same target list from the same geometry.
    UFUNCTION(BlueprintPure, Category="Combat|Melee")
    static float SortKey(const FVector& Origin, const FVector& Forward, const FVector& TargetLocation);

    // Server-only. Overlaps pawns in a sphere of Params.RangeCm, keeps those
    // inside the arc, drops the instigator, and returns them in SortKey order.
    static TArray<AActor*> SweepTargets(const UWorld* World, AActor* Instigator, const FBreakerMeleeSweepParams& Params);
};
