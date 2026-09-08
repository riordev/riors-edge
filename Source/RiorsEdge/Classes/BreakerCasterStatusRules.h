#pragma once
#include "CoreMinimal.h"
#include "Progression/BreakerProgressionTypes.h"
enum class EBreakerDamageFamily : uint8;
namespace BreakerCasterStatusRules
{
    // Called at accepted application, independently of physical Core rules captured at emission.
    void SnapshotPhysicalApplication(FBreakerStatusApplicationSpec& Spec, EBreakerDamageFamily Family, AActor* Source);
}
