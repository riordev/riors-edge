#pragma once
#include "CoreMinimal.h"
#include "Progression/BreakerCoreTree.h"
#include "Progression/BreakerProgressionTypes.h"
#include <initializer_list>

namespace BreakerCoreRoster
{
    RIORSEDGE_API FBreakerNodeEffect Effect(EBreakerNodeStatTarget Target, EBreakerNodeStatBucket Bucket, float Value);
    RIORSEDGE_API UBreakerProgressionNode* Node(UObject* Outer, const TCHAR* Id, const TCHAR* Name,
        const TCHAR* Description, std::initializer_list<FBreakerNodeEffect> Effects = {},
        std::initializer_list<const TCHAR*> Tags = {});
    RIORSEDGE_API void AppendWeapon(UObject* Outer, TArray<FBreakerCoreWedgeDefinition>& Out);
    // Candidate authoring is isolated from the live getter and save version.
    // Activate only with the complete roster and frozen-cost migration.
    RIORSEDGE_API UBreakerProgressionTree* BuildCandidate(UObject* Outer, FString& Error);
}
