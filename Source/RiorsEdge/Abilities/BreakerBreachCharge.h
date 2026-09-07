#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreakerBreachCharge.generated.h"

DECLARE_DELEGATE_OneParam(FBreakerChargeDetonated, FVector);

// One server-owned fuse per placed charge. Its replicated body is also the
// readable location of a sticky charge; no detached world-space glow remains.
UCLASS()
class RIORSEDGE_API ABreakerBreachCharge : public AActor
{
    GENERATED_BODY()
public:
    ABreakerBreachCharge();
    void Arm(AActor* Caster, float FuseSeconds, AActor* StickyTarget);
    void DetonateNow();
    FBreakerChargeDetonated OnDetonated;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    UFUNCTION() void CancelForOwnerDeath();
    void UpdateStickyLocation();
    TWeakObjectPtr<AActor> FollowTarget;
    FVector LocalImpact = FVector::ZeroVector;
    FTimerHandle FuseTimer;
    bool bResolved = false;
};
