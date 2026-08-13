#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/BreakerItemTypes.h"
#include "BreakerEquipmentComponent.generated.h"

class UBreakerAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBreakerEquipmentChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerItemAcquired, const FBreakerItemInstance&, Item);

// Owns the eight equipment slots plus a simple backpack, and folds equipped
// affixes into the attribute set. Aggregation rule: flat values sum, then
// the single additive Increased bucket applies once per stat. More
// multipliers are reserved for tree and Anomalous rule rewrites.
UCLASS(ClassGroup=Items, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerEquipmentComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerEquipmentComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Equipment") bool EquipItem(const FBreakerItemInstance& Item);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Equipment") bool UnequipSlot(EBreakerEquipSlot Slot);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Equipment") void AddToBackpack(const FBreakerItemInstance& Item);
    // Moves a backpack item into its slot; whatever was equipped there
    // returns to the backpack.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Equipment") bool EquipFromBackpack(const FGuid& ItemId);
    // Destroys a single backpack item outright. Returns false when the id is
    // not in the backpack (or on a client).
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Equipment") bool DiscardFromBackpack(const FGuid& ItemId);
    // Bulk cleanup: destroys every backpack item strictly below MinimumKept
    // and returns how many were removed. Equipped gear is never touched.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Equipment") int32 DiscardBackpackBelowRarity(EBreakerItemRarity MinimumKept);
    // Playtest helper: rolls one Exceptional item per equip slot at the given
    // item level and equips it, so TTK passes start from a full loadout.
    // Whatever was equipped goes back to the backpack the usual way.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Equipment") void DevGrantTestGear(int32 ItemLevel);
    UFUNCTION(BlueprintPure, Category="Equipment") bool GetEquippedItem(EBreakerEquipSlot Slot, FBreakerItemInstance& OutItem) const;
    UFUNCTION(BlueprintPure, Category="Equipment") const TArray<FBreakerItemInstance>& GetBackpack() const { return Backpack; }
    UFUNCTION(BlueprintPure, Category="Equipment") const TArray<FBreakerItemInstance>& GetEquipped() const { return Equipped; }
    // Save/load path: replaces both containers wholesale and recalculates.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Equipment") void RestoreState(const TArray<FBreakerItemInstance>& NewEquipped, const TArray<FBreakerItemInstance>& NewBackpack);
    UFUNCTION(BlueprintPure, Category="Equipment") const FBreakerEquipmentStats& GetStats() const { return CachedStats; }

    // Pure aggregation over any item set; the component wraps this for its
    // own equipped list so tests can exercise the math directly.
    static FBreakerEquipmentStats AggregateStats(const TArray<FBreakerItemInstance>& Items);

    UPROPERTY(BlueprintAssignable, Category="Equipment") FBreakerEquipmentChanged OnEquipmentChanged;
    UPROPERTY(BlueprintAssignable, Category="Equipment") FBreakerItemAcquired OnItemAcquired;

protected:
    UFUNCTION() void OnRep_Equipped();

private:
    UPROPERTY(ReplicatedUsing=OnRep_Equipped) TArray<FBreakerItemInstance> Equipped;
    UPROPERTY(Replicated) TArray<FBreakerItemInstance> Backpack;
    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;
    FBreakerEquipmentStats CachedStats;
    float BaseMaxHealth = -1.0f;
    float BaseMaxClassResource = -1.0f;
    float BaseCriticalChance = -1.0f;
    float BaseCriticalMultiplier = -1.0f;
    float BaseMoveSpeed = -1.0f;

    void RecalculateStats();
    void ApplyStatsToAttributes();
};
