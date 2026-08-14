#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Items/BreakerForgeLibrary.h"
#include "Items/BreakerItemRules.h"
#include "Items/BreakerItemTypes.h"
#include "BreakerEquipmentComponent.generated.h"

class UBreakerAttributeSet;
struct FBreakerHitContext;

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
    // Playtest helper: rolls every legendary at the given item level into the
    // backpack. Anomalous is ~0.5% of drops and only three slots have one, so
    // without this the three build-defining items are unreachable in a session
    // — which is the same "exists but cannot be found" failure as an affix with
    // no consumer, one step removed.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Equipment") void DevGrantLegendaries(int32 ItemLevel);
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
    // 4.1: Aberrant 3, Anomalous 1; everything below them is unlimited. O37
    // reaffirms O11's numbers and separates the Anomalous axis from the
    // legendary one — see CountEquippedOfRarity's comment and
    // CountEquippedLegendaries below.
    static int32 EquipLimitForRarity(EBreakerItemRarity Rarity);

    // How many equipped pieces are of this rarity. O37: a legendary rolls
    // Anomalous (O32) but sits on its own equip-cap axis, so it is EXCLUDED
    // here when Rarity is Anomalous — CountEquippedLegendaries answers that
    // question instead, and the two never double-count the same piece.
    UFUNCTION(BlueprintPure, Category="Equipment") int32 CountEquippedOfRarity(EBreakerItemRarity Rarity) const;
    // Companion to CountEquippedOfRarity for the legendary axis O37 splits
    // out. Capped at one by the same displacement mechanism as every other
    // rarity cap (PreviewEquipAgainst/EquipItem never refuse; they disclose
    // and displace the weakest piece on the axis).
    UFUNCTION(BlueprintPure, Category="Equipment") int32 CountEquippedLegendaries() const;

    // O37: exactly 1 equipped legendary, 1 non-legendary Anomalous, 3
    // Aberrant. Equip-time displacement (PreviewEquipAgainst/EquipItem) never
    // lets a live component exceed these, so this validator's job is a save
    // load, a hand-edited fixture, or a content-authoring check finding MORE
    // than the cap — the one thing that should never happen live. Pure and
    // static so it needs no component, actor, or world.
    UFUNCTION(BlueprintPure, Category="Equipment|Validation")
    static bool ValidateEquipCaps(const TArray<FBreakerItemInstance>& Items, FText& OutFailureReason);

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
    // Conditional lines pay out only for conditions active in Conditions; the
    // default empty state means "standing still", so every call site that
    // predates the conditional family keeps its exact previous behaviour.
    // Rule rewrites carried by the equipped set are resolved and applied HERE,
    // inside the same function, rather than by a second pass somewhere else.
    // That is the whole reason an Anomalous rewrite reaches gameplay at all:
    // this function's output is what the attribute set folds and what
    // UBreakerCombatComponent reads, so a rewrite expressed here cannot be a
    // line of text on a card.
    static FBreakerEquipmentStats AggregateStats(const TArray<FBreakerItemInstance>& Items, FBreakerAttributeContribution* OutContribution = nullptr,
        const FBreakerBuildConditionState& Conditions = FBreakerBuildConditionState());

    // The rules the equipped set currently imposes. Exposed so the inventory
    // can print them and so a test can assert on the resolved set rather than
    // inferring it from a composed number.
    UFUNCTION(BlueprintPure, Category="Equipment|Rules")
    TArray<EBreakerItemRule> GetActiveRules() const { return CachedStats.ActiveRules; }

    // ---- The Forge --------------------------------------------------------
    // Item agency, gated on standing at one exactly like the respec is. The
    // arithmetic lives in UBreakerForgeLibrary as pure functions; this is the
    // authority-checked, wallet-owning wrapper.
    UFUNCTION(BlueprintPure, Category="Equipment|Forge") const FBreakerForgeWallet& GetForgeWallet() const { return ForgeWallet; }
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Equipment|Forge") void GrantForgeCurrency(EBreakerForgeCurrency Currency, int32 Amount);
    // Destroys a backpack item and pays its salvage value into the wallet. This
    // is the ONLY currency source, which is what gives the discard pile a
    // purpose it did not have: DiscardFromBackpack still exists and still pays
    // nothing, so "melt" and "bin" are different verbs.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Equipment|Forge") bool SalvageFromBackpack(const FGuid& ItemId);
    // Bulk salvage, sharing its predicate with DiscardBackpackBelowRarity so
    // the two cannot disagree about which items are junk.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Equipment|Forge") int32 SalvageBackpackBelowRarity(EBreakerItemRarity MinimumKept);
    // The three verbs, applied to an item held anywhere (backpack or equipped)
    // and identified by id, because an item the player is wearing is exactly
    // the item they most want to improve.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Equipment|Forge") EBreakerForgeResult TemperItem(const FGuid& ItemId, int32 AffixIndex, bool bIsAtForge);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Equipment|Forge") EBreakerForgeResult ReforgeItem(const FGuid& ItemId, bool bIsAtForge);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Equipment|Forge") EBreakerForgeResult AttuneItem(const FGuid& ItemId, bool bIsAtForge);

    // The movement/momentum conditions this component last folded in. The tick
    // re-evaluates and only resubmits when the SET changes, so gear damage
    // tracks state without recomputing eight items' worth of affixes a frame.
    const FBreakerBuildConditionState& GetActiveConditions() const { return ActiveConditions; }

    // Binds the attribute set this component contributes to. BeginPlay calls
    // it with the set found on the owner's ability system; tests call it with
    // a standalone set. Capturing the bases is the attribute set's job.
    void BindAttributes(UBreakerAttributeSet* InAttributes);

    // Subscribes to the owner's combat events. Called from BeginPlay, and
    // exposed for the same reason BindAttributes is: an affix that only pays
    // out when a real actor in a real world lands a real kill is exactly the
    // kind of line that ships broken, so the whole chain — bind, broadcast,
    // heal — has to be exercisable in automation. Idempotent.
    void BindCombatEvents();

    // This layer's current offer, exactly as submitted.
    const FBreakerAttributeContribution& GetAttributeContribution() const { return CachedContribution; }

    UPROPERTY(BlueprintAssignable, Category="Equipment") FBreakerEquipmentChanged OnEquipmentChanged;
    UPROPERTY(BlueprintAssignable, Category="Equipment") FBreakerItemAcquired OnItemAcquired;

protected:
    UFUNCTION() void OnRep_Equipped();
    // Bound to the owner's UBreakerCombatComponent::OnKillDealt. This is how
    // Health on Kill and Resource on Kill reach gameplay: an amount paid at an
    // event has no attribute to live in, so it needs a listener. Bound once in
    // BeginPlay and never rebound, so a recalculation cannot double-subscribe.
    UFUNCTION() void HandleKillDealt(const FBreakerHitContext& Hit);

private:
    UPROPERTY(ReplicatedUsing=OnRep_Equipped) TArray<FBreakerItemInstance> Equipped;
    UPROPERTY(Replicated) TArray<FBreakerItemInstance> Backpack;
    UPROPERTY(Replicated) FBreakerForgeWallet ForgeWallet;
    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;
    FBreakerEquipmentStats CachedStats;
    // No base-value cache lives here any more. The attribute set owns the one
    // true base; this component only ever submits a contribution, which is why
    // gear and skill nodes now stack instead of overwriting each other.
    FBreakerAttributeContribution CachedContribution;
    FBreakerBuildConditionState ActiveConditions;

    // Re-reads the owner's movement/momentum state and recalculates only if the
    // active set actually moved. Called from the tick.
    void RefreshBuildConditions();
    void RecalculateStats();
    void ApplyStatsToAttributes();
    bool HasAttributeAuthority() const;
    // Finds a held item by id in either container. Returns null when the id is
    // unknown; the caller re-equips if the item it mutated was worn.
    FBreakerItemInstance* FindHeldItem(const FGuid& ItemId, bool& bOutEquipped);
    // Rebuilds attributes and broadcasts after a Forge operation mutated an
    // item in place. Equipped gear has to re-fold; a backpack item only has to
    // repaint, but the same call covers both and cannot forget the first.
    void OnHeldItemMutated(bool bWasEquipped);
};
