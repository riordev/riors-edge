#pragma once

#include "CoreMinimal.h"

class UBreakerProgressionNode;
class UBreakerProgressionTree;

struct FBreakerCoreLaneDefinition
{
    const UBreakerProgressionNode* Minor = nullptr;
    const UBreakerProgressionNode* Notable = nullptr;
};

// Caller supplies authored identities, descriptions and effects. Construction
// copies those nodes and owns only their structural metadata and purchase gates.
struct FBreakerCoreWedgeDefinition
{
    FName Id;
    FName Sector;
    bool bMajor = false;
    const UBreakerProgressionNode* Gateway = nullptr;
    TArray<FBreakerCoreLaneDefinition> Lanes;
    TArray<const UBreakerProgressionNode*> Links;
    const UBreakerProgressionNode* Convergence = nullptr;
    const UBreakerProgressionNode* Keystone = nullptr;
};

namespace BreakerCoreTree
{
    // Invalid input returns nullptr before creating output. No roster, effects,
    // global singleton or save activation is supplied by this utility.
    RIORSEDGE_API UBreakerProgressionTree* Build(UObject* Outer, FName TreeId,
        const FText& Title, const TArray<FBreakerCoreWedgeDefinition>& Wedges, FString& Error);
}
