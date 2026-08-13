#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerProgressionNode.generated.h"

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerNodePrerequisite
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName NodeId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 RequiredRank = 1;
};

UCLASS(BlueprintType)
class RIORSEDGE_API UBreakerProgressionNode : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(TEXT("ProgressionNode"), NodeId);
    }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FName NodeId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FText DisplayName;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity", meta=(MultiLine="true")) FText Description;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FGameplayTagContainer NodeTags;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rules") EBreakerPointCurrency Currency = EBreakerPointCurrency::CorePoints;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rules") EBreakerClassId RequiredClass = EBreakerClassId::None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rules", meta=(ClampMin="1")) int32 MaxRank = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rules", meta=(ClampMin="1")) int32 CostPerRank = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rules") TArray<FBreakerNodePrerequisite> Prerequisites;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rules") TArray<FName> MutuallyExclusiveNodeIds;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rules") bool bCornerstone = false;
    // Presentation/layout tier (1-5). The purchasable gate is
    // RequiredTreeInvestment; Tier exists so the UI can lay the tree out and
    // so content can be authored against the design doc's tier tables.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rules", meta=(ClampMin="1")) int32 Tier = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rules", meta=(ClampMin="0")) int32 RequiredTreeInvestment = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grants") TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grants") TArray<TSubclassOf<UGameplayEffect>> GrantedEffects;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grants") TArray<FName> GrantedAbilityIds;
    // Stat contribution per owned rank; aggregated by the progression
    // component into FBreakerNodeStats.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grants") TArray<FBreakerNodeEffect> Effects;
    // Rule rewrites and verb grants that cannot be a stat. Owning systems
    // query them through UBreakerProgressionComponent::HasNodeTag.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grants") FGameplayTagContainer GrantedTags;
};
