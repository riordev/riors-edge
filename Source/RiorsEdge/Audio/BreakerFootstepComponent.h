#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BreakerFootstepComponent.generated.h"

// Distance earns steps; elapsed time only limits their maximum cadence.
struct FBreakerFootstepCadence
{
    FVector Previous = FVector::ZeroVector;
    float Distance = 0, Seconds = 0;
    bool bInitialized = false;
    bool Advance(FVector Position, float Speed, float Delta, bool bAllowed)
    {
        const float Travel = bInitialized ? FVector::Dist2D(Position, Previous) : 0;
        const bool bContinuous = bInitialized;
        Previous = Position; bInitialized = true;
        if (!bContinuous || !bAllowed || !FMath::IsFinite(Delta) || Delta <= 0 || Delta > .25f
            || !FMath::IsFinite(Speed) || Speed < 10 || !FMath::IsFinite(Travel) || Travel < .1f
            || Travel > FMath::Max(80.0f, Speed * Delta * 1.75f + 20.0f))
        { Distance = Seconds = 0; return false; }
        Seconds += Delta;
        Distance = FMath::Min(170.0f, Distance + Travel);
        if (Distance < 170.0f || Seconds < .28f) return false;
        Distance = Seconds = 0;
        return true;
    }
};

UCLASS(ClassGroup=Audio)
class RIORSEDGE_API UBreakerFootstepComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UBreakerFootstepComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
    FBreakerFootstepCadence Cadence;
    uint32 LastContinuity = 0;
};
