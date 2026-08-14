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
    // Which Core constellation this node belongs to (Precision, Volley,
    // Affliction, Bulwark, Kinesis, Velocity, Elements). Only meaningful for
    // Core tree nodes; class branch nodes leave this None — a branch is not a
    // constellation. Membership used to be inferred ONLY from the NodeId
    // string prefix (UI/BreakerMenu.cpp's hardcoded per-constellation cluster
    // list), which is exactly what let the Velocity constellation's six nodes
    // fall into the board's UNMAPPED catch-all the day they were authored
    // without a matching UI entry: a field is authoritative where a naming
    // convention is silent. The UI is another lane's territory this pass —
    // this is the data the consumer gets wired to at integration.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FName Constellation = NAME_None;

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
