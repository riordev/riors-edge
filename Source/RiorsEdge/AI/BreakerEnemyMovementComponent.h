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
    EBreakerLocomotionMode Drive(const FVector& Direction, float SpeedScale, AActor* Target,
        float DistanceToTarget, float AttackRange, float MoveSpeed);

    // Blocking impacts against upright world geometry since the last reset —
    // the number the nav probe reports as "touches". A floor contact does not
    // count; the ground snap owns the floor.
    int32 GetWorldTouchCount() const { return WorldTouchCount; }
    EBreakerLocomotionMode GetLastMode() const { return LastMode; }

    // A parked or revived body starts still, with nothing counted against it.
    void ResetForRevive();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.0f, const FVector& MoveDelta = FVector::ZeroVector) override;

protected:
    // True when world-static geometry stands on the straight line from the
    // pawn to the target. One line trace per call.
    bool IsClosingLineBlocked(const AActor* Target) const;

    // The ground snap, moved here verbatim from ABreakerEnemy::Tick: trace
    // down, plant the capsule base on whatever is below — snap down
    // instantly, step up smoothly, so slabs read as steps rather than
    // teleports.
    void SnapToGround(float DeltaTime);

    int32 WorldTouchCount = 0;
    EBreakerLocomotionMode LastMode = EBreakerLocomotionMode::Idle;
};
