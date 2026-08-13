#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerProgressionComponent.generated.h"

class UBreakerClassDefinition;
class UBreakerProgressionNode;
class UBreakerProgressionTree;

UCLASS(ClassGroup=Progression, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerProgressionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerProgressionComponent();

    UFUNCTION(BlueprintCallable, Category="Progression") bool ChoosePermanentClass(const UBreakerClassDefinition* ClassDefinition);
    // Selection framework path while class Data Assets do not exist yet:
    // locks the permanent class by id alone. Same one-way rule.
    UFUNCTION(BlueprintCallable, Category="Progression") bool ChoosePermanentClassById(EBreakerClassId ClassId);
    // Dev-only escape hatch behind the menu's dev toggle: swaps the class
    // regardless of the permanent-selection rule. Never ship a path to this.
    UFUNCTION(BlueprintCallable, Category="Progression|Dev") void DevForceClass(EBreakerClassId ClassId) { State.PermanentClass = ClassId; }
    UFUNCTION(BlueprintCallable, Category="Progression") bool PurchaseNode(const UBreakerProgressionTree* Tree, FName NodeId, FText& OutFailureReason);
    UFUNCTION(BlueprintCallable, Category="Progression") bool EquipAbility(EBreakerAbilitySlot Slot, FName AbilityId, FText& OutFailureReason);
    UFUNCTION(BlueprintCallable, Category="Progression") bool RespecAtForge(EBreakerPointCurrency Currency, bool bIsAtForge, FText& OutFailureReason);
    UFUNCTION(BlueprintPure, Category="Progression") int32 GetNodeRank(FName NodeId, EBreakerPointCurrency Currency) const;
    UFUNCTION(BlueprintPure, Category="Progression") const FBreakerProgressionState& GetProgressionState() const { return State; }
    UFUNCTION(BlueprintCallable, Category="Progression") void LoadProgressionState(const FBreakerProgressionState& NewState);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Progression") TObjectPtr<UBreakerClassDefinition> ClassDefinition;

private:
    UPROPERTY(VisibleInstanceOnly, Category="Progression") FBreakerProgressionState State;

    int32 GetTreeInvestment(const UBreakerProgressionTree* Tree) const;
    int32 GetRefundValue(EBreakerPointCurrency Currency) const;
    const UBreakerProgressionNode* FindOwnedNodeDefinition(FName NodeId, EBreakerPointCurrency Currency) const;
    bool IsAbilityUnlocked(FName AbilityId) const;
    TArray<FBreakerNodeRank>& RanksFor(EBreakerPointCurrency Currency);
    const TArray<FBreakerNodeRank>& RanksFor(EBreakerPointCurrency Currency) const;
};
