#pragma once
#include "CoreMinimal.h"
#include "Progression/BreakerProgressionTypes.h"
enum class EBreakerDamageFamily : uint8;
struct FBreakerDamageRequest;
struct FBreakerDamageResult;
namespace BreakerCasterStatusRules
{
    float RotLifetimeMultiplier(AActor* Source);
    float RotCriticalBudgetMultiplier(const FBreakerDamageRequest& Request, const FBreakerDamageResult& Result);
    // Called at accepted application, independently of physical Core rules captured at emission.
    void SnapshotPhysicalApplication(FBreakerStatusApplicationSpec& Spec, EBreakerDamageFamily Family, AActor* Source);
}
