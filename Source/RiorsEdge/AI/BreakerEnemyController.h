#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BreakerEnemyController.generated.h"

// The enemy's controller (NAV-1). No perception, no behaviour tree, no
// focus: the band classifier inside each archetype's TickEngagedBehaviour is
// the brain, and this class owns exactly one verb — path to a goal the mover
// hands it, and stop when the mover stops asking.
//
// It also guarantees the level has a navmesh to path on. Every level's
// geometry is spawned at runtime, so on possession the controller asks
// BreakerNavBounds to cover whatever static meshes exist; nothing per map is
// authored anywhere.
UCLASS()
class RIORSEDGE_API ABreakerEnemyController : public AAIController
{
    GENERATED_BODY()

public:
    ABreakerEnemyController(const FObjectInitializer& ObjectInitializer);

    // Path to Goal, re-planning only when the goal has moved
    // ReplanDistanceCm or the previous move ended. False means the request
    // was refused (no navmesh yet, no path) and the caller steers this frame.
    bool Chase(const FVector& Goal, float AcceptanceRadius);
    void StopChase();
    bool IsChasing() const;

protected:
    virtual void OnPossess(APawn* InPawn) override;

private:
    FVector LastGoal = FVector::ZeroVector;
    bool bHasGoal = false;
};
