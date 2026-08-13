#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Attributes/BreakerAttributeAggregation.h"
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

    // ---- Disclosure queries -----------------------------------------------
    // These answer "what happens if I equip this" BEFORE the click, so the
    // loadout screen can state consequences instead of the player discovering
    // them. Comparison and cap arithmetic are game rules and live here, not in
    // Slate; every one of them is usable from a ground-loot popup or a vendor
    // screen without touching the loadout widget.

    // Equipped cap for a rarity. INDEX_NONE means uncapped. O11 / master sheet
    // 4.1: Aberrant 3, Anomalous 1; everything below them is unlimited.
    static int32 EquipLimitForRarity(EBreakerItemRarity Rarity);

    // How many equipped pieces are of this rarity. The header limit counters
    // read this instead of walking GetEquipped() themselves.
    UFUNCTION(BlueprintPure, Category="Equipment") int32 CountEquippedOfRarity(EBreakerItemRarity Rarity) const;

    // How many backpack items DiscardBackpackBelowRarity would destroy. Shares
    // its predicate with the discard itself, so the number the confirmation
    // modal states and the number destroyed cannot drift apart.
    UFUNCTION(BlueprintPure, Category="Equipment") int32 CountBackpackBelowRarity(EBreakerItemRarity MinimumKept) const;

    // The full consequence of equipping this item against the current loadout.
    UFUNCTION(BlueprintPure, Category="Equipment") FBreakerEquipPreview PreviewEquip(const FBreakerItemInstance& Candidate) const;

    // Pure form of the same rule over any equipped set, so the cap and the
    // displacement choice are testable with no component, actor, or world.
    static FBreakerEquipPreview PreviewEquipAgainst(const TArray<FBreakerItemInstance>& EquippedItems, const FBreakerItemInstance& Candidate);

    // Per-affix comparison of Candidate against Reference. Matching is by
    // (stat target, bucket), not by affix id: two affixes that raise the same
    // stat the same way are one number to the player, and a flat +Health is
    // not comparable against an Increased Health percentage. An invalid
    // Reference (empty slot) makes every line an improvement.
    static TArray<FBreakerAffixComparison> CompareAffixes(const FBreakerItemInstance& Candidate, const FBreakerItemInstance& Reference);

    // Pure aggregation over any item set; the component wraps this for its
    // own equipped list so tests can exercise the math directly. The optional
    // out-contribution is this layer's offer to the unified application path
    // in UBreakerAttributeSet, built from the same raw buckets so nothing has
    // to be reverse-engineered out of the composed multipliers.
    static FBreakerEquipmentStats AggregateStats(const TArray<FBreakerItemInstance>& Items, FBreakerAttributeContribution* OutContribution = nullptr);

    // Binds the attribute set this component contributes to. BeginPlay calls
    // it with the set found on the owner's ability system; tests call it with
    // a standalone set. Capturing the bases is the attribute set's job.
    void BindAttributes(UBreakerAttributeSet* InAttributes);

    // This layer's current offer, exactly as submitted.
    const FBreakerAttributeContribution& GetAttributeContribution() const { return CachedContribution; }

    UPROPERTY(BlueprintAssignable, Category="Equipment") FBreakerEquipmentChanged OnEquipmentChanged;
    UPROPERTY(BlueprintAssignable, Category="Equipment") FBreakerItemAcquired OnItemAcquired;

protected:
    UFUNCTION() void OnRep_Equipped();

private:
    UPROPERTY(ReplicatedUsing=OnRep_Equipped) TArray<FBreakerItemInstance> Equipped;
    UPROPERTY(Replicated) TArray<FBreakerItemInstance> Backpack;
    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;
    FBreakerEquipmentStats CachedStats;
    // No base-value cache lives here any more. The attribute set owns the one
    // true base; this component only ever submits a contribution, which is why
    // gear and skill nodes now stack instead of overwriting each other.
    FBreakerAttributeContribution CachedContribution;

    void RecalculateStats();
    void ApplyStatsToAttributes();
    bool HasAttributeAuthority() const;
};
