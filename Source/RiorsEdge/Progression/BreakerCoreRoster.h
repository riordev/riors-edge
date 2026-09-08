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
    RIORSEDGE_API void AppendDefence(UObject* Outer, TArray<FBreakerCoreWedgeDefinition>& Out);
    RIORSEDGE_API void AppendAbility(UObject* Outer, TArray<FBreakerCoreWedgeDefinition>& Out);
    RIORSEDGE_API void AppendStatus(UObject* Outer, TArray<FBreakerCoreWedgeDefinition>& Out);
    RIORSEDGE_API void AppendMovement(UObject* Outer, TArray<FBreakerCoreWedgeDefinition>& Out);
    RIORSEDGE_API void AppendUtility(UObject* Outer, TArray<FBreakerCoreWedgeDefinition>& Out);
    // Builds the full roster; the live getter owns caching and migration activation.
    RIORSEDGE_API UBreakerProgressionTree* BuildCandidate(UObject* Outer, FString& Error);
}
