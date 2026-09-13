#pragma once

#include "CoreMinimal.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "AI/BreakerLocomotionMath.h"
#include "BreakerEnemyMovementComponent.generated.h"

// The enemy's mover (NAV-1). A floating pawn movement — no gravity, which is
// the feel every enemy shipped with when Tick moved them by offset — that
// takes the behaviour's per-frame direction and speed scale through Drive and
// either steers along it or hands the frame to the controller's path.
//
// Owned by NAV. Combat/BreakerEnemy calls Drive once per authoritative tick
// and touches nothing else here; the ground snap that used to close Tick
// lives in TickComponent below, unchanged.
UCLASS(ClassGroup=(Breaker), meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerEnemyMovementComponent : public UFloatingPawnMovement
{
    GENERATED_BODY()

public:
    UBreakerEnemyMovementComponent(const FObjectInitializer& ObjectInitializer);

    // The behaviour's answer for this frame. Direction is world-space and
    // already normalised (or zero for "hold"); SpeedScale multiplies MoveSpeed
    // exactly as Tick used to. Target may be null (patrol). Returns the mode
    // that was chosen so a caller can print it.
    //
    // bHasGoal / Goal is the archetype's "go HERE, not at the player" channel
    // (NAV-2). With a goal the closing line is traced to the goal, the
    // acceptance is the pawn's own capsule radius — the number PATROL already
    // arrives by — and a path, when one is needed, is requested to the goal.
    // The target is still passed so the trace can ignore it.
    EBreakerLocomotionMode Drive(const FVector& Direction, float SpeedScale, AActor* Target,
        float DistanceToTarget, float AttackRange, float MoveSpeed,
        bool bHasGoal = false, const FVector& Goal = FVector::ZeroVector);

    // Blocking impacts against upright world geometry since the last reset —
    // the number the nav probe reports as "touches". A floor contact does not
    // count; the ground snap owns the floor.
    int32 GetWorldTouchCount() const { return WorldTouchCount; }
    EBreakerLocomotionMode GetLastMode() const { return LastMode; }
    bool IsBlockedHold() const { return bBlockedHold; }
    FVector GetMeasuredGroundVelocity() const { return MeasuredGroundVelocity; }
    FVector GetAvoidanceHeading() const { return AvoidanceHeading; }
    int32 GetClearanceHoldCount() const { return ClearanceHoldCount; }
    // The leg the path follower is walking this body along, as a unit
    // direction, or zero when it holds no path. The facing rule reads this
    // and not the velocity, which the speed scale can erase.
    FVector GetPathHeading() const;

    // A parked or revived body starts still, with nothing counted against it.
    void ResetForRevive();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed) override;
    virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.0f, const FVector& MoveDelta = FVector::ZeroVector) override;

protected:
    // Sweep the capsule's clearance through static geometry, ignoring pawns.
    bool IsClosingLineBlocked(const FVector& To, const AActor* Ignore) const;
    bool SweepStep(const FVector& Delta, FHitResult& Hit, bool bIncludePawns) const;
    void ConstrainNextStep(float DeltaTime);

    // The ground snap, moved here verbatim from ABreakerEnemy::Tick: trace
    // down, plant the capsule base on whatever is below — snap down
    // instantly, step up smoothly, so slabs read as steps rather than
    // teleports.
    void SnapToGround(float DeltaTime);

    int32 WorldTouchCount = 0;
    int32 ClearanceHoldCount = 0;
    FVector MeasuredGroundVelocity = FVector::ZeroVector;
    FVector AvoidanceHeading = FVector::ZeroVector;
    FVector PreviousClientLocation = FVector::ZeroVector;
    bool bHasClientLocation = false;
    bool bBlockedHold = false;
    EBreakerLocomotionMode LastMode = EBreakerLocomotionMode::Idle;
};
