#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerProgressionComponent.generated.h"

class UBreakerAttributeSet;
class UBreakerClassDefinition;
class UBreakerProgressionNode;
class UBreakerProgressionTree;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBreakerProgressionChanged);

UCLASS(ClassGroup=Progression, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerProgressionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerProgressionComponent();
    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Category="Progression") bool ChoosePermanentClass(const UBreakerClassDefinition* ClassDefinition);
    // Selection framework path while class Data Assets do not exist yet:
    // locks the permanent class by id alone. Same one-way rule.
    UFUNCTION(BlueprintCallable, Category="Progression") bool ChoosePermanentClassById(EBreakerClassId ClassId);
    // Dev-only escape hatch behind the menu's dev toggle: swaps the class
    // regardless of the permanent-selection rule. Never ship a path to this.
    UFUNCTION(BlueprintCallable, Category="Progression|Dev") void DevForceClass(EBreakerClassId ClassId);
    UFUNCTION(BlueprintCallable, Category="Progression") bool PurchaseNode(const UBreakerProgressionTree* Tree, FName NodeId, FText& OutFailureReason);
    // Same validation PurchaseNode runs, without spending. The tree UI calls
    // this per node to decide enabled/disabled state and its tooltip reason.
    UFUNCTION(BlueprintCallable, Category="Progression") bool CanPurchaseNode(const UBreakerProgressionTree* Tree, FName NodeId, FText& OutFailureReason) const;
    UFUNCTION(BlueprintCallable, Category="Progression") bool EquipAbility(EBreakerAbilitySlot Slot, FName AbilityId, FText& OutFailureReason);
    UFUNCTION(BlueprintCallable, Category="Progression") bool RespecAtForge(EBreakerPointCurrency Currency, bool bIsAtForge, FText& OutFailureReason);
    UFUNCTION(BlueprintPure, Category="Progression") int32 GetNodeRank(FName NodeId, EBreakerPointCurrency Currency) const;
    UFUNCTION(BlueprintPure, Category="Progression") int32 GetUnspentPoints(EBreakerPointCurrency Currency) const;
    UFUNCTION(BlueprintPure, Category="Progression") int32 GetTreeInvestment(const UBreakerProgressionTree* Tree) const;
    UFUNCTION(BlueprintPure, Category="Progression") const FBreakerProgressionState& GetProgressionState() const { return State; }
    UFUNCTION(BlueprintCallable, Category="Progression") void LoadProgressionState(const FBreakerProgressionState& NewState);

    // Every tree this character may spend in, fallback content included. The
    // UI enumerates trees here and nodes through UBreakerProgressionTree.
    UFUNCTION(BlueprintPure, Category="Progression") TArray<UBreakerProgressionTree*> GetAvailableTrees() const;

    // Aggregated node output. Combat and movement read these rather than
    // walking node ranks themselves.
    UFUNCTION(BlueprintPure, Category="Progression") const FBreakerNodeStats& GetNodeStats() const { return CachedStats; }
    // Rule rewrites and verb grants that are not expressible as a stat.
    UFUNCTION(BlueprintPure, Category="Progression") bool HasNodeTag(FGameplayTag Tag) const { return CachedStats.GrantedTags.HasTag(Tag); }
    // NOTE for the combat pass: UBreakerCombatComponent should add these to
    // its DodgeChance/BlockChance before rolling. This component deliberately
    // does not write to it — that wiring belongs to the combat owner.
    UFUNCTION(BlueprintPure, Category="Progression") float GetDodgeChanceBonus() const { return CachedStats.DodgeChanceBonus; }
    UFUNCTION(BlueprintPure, Category="Progression") float GetBlockChanceBonus() const { return CachedStats.BlockChanceBonus; }
    UFUNCTION(BlueprintPure, Category="Progression") float GetMoveSpeedMultiplier() const { return CachedStats.MoveSpeedMultiplier; }
    UFUNCTION(BlueprintPure, Category="Progression") float GetSlideSpeedMultiplier() const { return CachedStats.SlideSpeedMultiplier; }
    UFUNCTION(BlueprintPure, Category="Progression") float GetAirControlMultiplier() const { return CachedStats.AirControlMultiplier; }

    // Playtest hook: hands the gym the slice point budget so trees can be
    // exercised without an XP loop. O2 PLACEHOLDER budget (XP §9).
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Progression|Playtest")
    void GrantPlaytestPoints(int32 ClassPoints, int32 CorePoints);
    // Seeds the slice budget whenever the point economy is empty (no ranks in
    // either currency and nothing unspent), and locks Swift only if no class
    // is chosen — so both a new gym pawn and an existing save written before
    // this seeding existed end up with something to spend. Called from
    // BeginPlay and again from LoadProgressionState; a no-op once anything has
    // actually been granted or spent.
    UFUNCTION(BlueprintCallable, Category="Progression|Playtest") void ApplySliceDefaultsIfFresh();

    // Pure aggregation over a rank set, mirroring
    // UBreakerEquipmentComponent::AggregateStats so tests can exercise the
    // math with no actor. Flat values sum, then one additive Increased
    // bucket per stat; More multipliers stay reserved for keystones (O3).
    static FBreakerNodeStats AggregateStats(const TArray<const UBreakerProgressionNode*>& Nodes, const TArray<FBreakerNodeRank>& Ranks);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Progression") TObjectPtr<UBreakerClassDefinition> ClassDefinition;
    UPROPERTY(BlueprintAssignable, Category="Progression") FBreakerProgressionChanged OnProgressionChanged;

private:
    UPROPERTY(VisibleInstanceOnly, Category="Progression") FBreakerProgressionState State;
    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;

    FBreakerNodeStats CachedStats;
    float BaseMaxHealth = -1.0f;
    float BaseCriticalChance = -1.0f;
    float BaseCriticalMultiplier = -1.0f;
    float BaseMoveSpeed = -1.0f;
    float BaseDamageOverTimeMultiplier = -1.0f;

    int32 GetRefundValue(EBreakerPointCurrency Currency) const;
    const UBreakerProgressionNode* FindOwnedNodeDefinition(FName NodeId, EBreakerPointCurrency Currency) const;
    void CollectKnownNodes(TArray<const UBreakerProgressionNode*>& OutNodes, EBreakerPointCurrency Currency) const;
    bool IsAbilityUnlocked(FName AbilityId) const;
    TArray<FBreakerNodeRank>& RanksFor(EBreakerPointCurrency Currency);
    const TArray<FBreakerNodeRank>& RanksFor(EBreakerPointCurrency Currency) const;
    void RecalculateStats();
    void ApplyStatsToAttributes();
};
