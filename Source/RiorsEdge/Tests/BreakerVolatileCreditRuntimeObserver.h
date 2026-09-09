#pragma once
#include "CoreMinimal.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerVolatileCreditRuntimeObserver.generated.h"

// O245's witness: every kill the PLAYER is credited with, kept whole so the
// test can read the author and the earner apart rather than trusting a count.
UCLASS()
class UBreakerVolatileCreditRuntimeObserver : public UObject
{
    GENERATED_BODY()
public:
    TArray<FBreakerHitContext> Kills;
    TArray<FBreakerHitContext> Hits;
    UFUNCTION() void OnKill(const FBreakerHitContext& Hit);
    UFUNCTION() void OnHit(const FBreakerHitContext& Hit);
};
