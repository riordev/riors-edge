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
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rules", meta=(ClampMin="0")) int32 CornerstoneInvestmentGate = 8;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Nodes") TArray<TObjectPtr<UBreakerProgressionNode>> Nodes;

    const UBreakerProgressionNode* FindNode(FName NodeId) const;
};
