#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerClassDefinition.generated.h"

class UBreakerProgressionTree;

UCLASS(BlueprintType)
class RIORSEDGE_API UBreakerClassDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(TEXT("ClassDefinition"), ClassAssetId);
    }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FName ClassAssetId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") EBreakerClassId ClassId = EBreakerClassId::None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FText DisplayName;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity", meta=(MultiLine="true")) FText Description;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resource") FGameplayTag ResourceTag;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Progression") TArray<TObjectPtr<UBreakerProgressionTree>> BranchTrees;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout") TArray<FName> StartingClassAbilityIds;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout") FName BaseUltimateId = NAME_None;
};
