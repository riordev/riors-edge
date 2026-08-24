#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerProgressionTree.generated.h"

class UBreakerProgressionNode;

UCLASS(BlueprintType)
class RIORSEDGE_API UBreakerProgressionTree : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(TEXT("ProgressionTree"), TreeId);
    }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FName TreeId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FText DisplayName;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rules") EBreakerPointCurrency Currency = EBreakerPointCurrency::CorePoints;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rules") EBreakerClassId RequiredClass = EBreakerClassId::None;
    // A CORE-ONLY CONCEPT (owner ruling). Doctrine trees zero it in MakeTree.
    //
    // THE RULE THIS FIELD EXISTS TO WARN ABOUT: a gate and the budget it gates
    // against are ONE NUMBER IN TWO PLACES. This one said 8 points spent in the
    // tree, which was affordable when a class tree drew on a per-level budget.
    // O111 set the doctrine wallet to 8 and did not move it, so every doctrine
    // keystone needed 8 + 3 = 11 out of 8 and none could be bought — the second
    // time this project has shipped unpurchasable keystones, both times from a
    // budget moving while the gate keyed to it stayed put.
    //
    // Picking a smaller number here would only re-arm it for the next budget
    // change, so the doctrine side does not have one at all; depth there is the
    // per-tier gate, which GateForTier derives rather than pins. Core keeps the
    // field because Core's atlas is large enough for a cornerstone to mean
    // something. WHOEVER SETS IT NEXT: the value is only correct relative to a
    // budget, so state that budget here, and note that
    // RiorsEdge.Progression.TreeDepthIsReachable walks every tree and will fail
    // if the two ever disagree again.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rules", meta=(ClampMin="0")) int32 CornerstoneInvestmentGate = 8;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Nodes") TArray<TObjectPtr<UBreakerProgressionNode>> Nodes;

    const UBreakerProgressionNode* FindNode(FName NodeId) const;
};
