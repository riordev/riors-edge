#pragma once
#include "CoreMinimal.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerRocketRicochetRuntimeObserver.generated.h"
UCLASS()
class UBreakerRocketRicochetRuntimeObserver : public UObject
{
    GENERATED_BODY()
public:
    TArray<FBreakerHitContext> Hits;
    int32 Explosions=0;
    FVector ExplosionLocation=FVector::ZeroVector;
    UFUNCTION() void OnHit(const FBreakerHitContext& Hit);
    UFUNCTION() void OnExploded(const FVector& Location,float Radius);
};
