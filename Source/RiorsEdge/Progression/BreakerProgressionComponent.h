#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Progression/BreakerProgressionTypes.h"
#include "Progression/BreakerExperience.h"
#include "BreakerProgressionComponent.generated.h"

class UBreakerAttributeSet;
class UBreakerClassDefinition;
class UBreakerProgressionNode;
class UBreakerProgressionTree;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBreakerProgressionChanged);
// NewLevel, and how many levels arrived at once — a single kill can cross more
// than one level early on, and a tell that says "level 2" when the player
// reached 4 is worse than no tell.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBreakerLevelGained, int32, NewLevel, int32, LevelsGained);

UCLASS(ClassGroup=Progression, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerProgressionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerProgressionComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

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
    // O37 subclass commitment: one-way (refuses if State.CommittedBranch is
    // already set), and BranchTreeId must name a class branch tree this
    // character can actually spend in (Core's RequiredClass==None trees do
    // not count — Core is not a subclass). RespecAtForge(ClassPoints, ...) is
    // the only way back to None. Committing unlocks that branch's
    // bCornerstone-flagged keystone tier in CanPurchaseNode; every ordinary
    // node of every branch stays freely purchasable regardless (O15 intact).
    UFUNCTION(BlueprintCallable, Category="Progression") bool CommitToBranch(FName BranchTreeId, FText& OutFailureReason);
    UFUNCTION(BlueprintPure, Category="Progression") int32 GetNodeRank(FName NodeId, EBreakerPointCurrency Currency) const;
    UFUNCTION(BlueprintPure, Category="Progression") int32 GetUnspentPoints(EBreakerPointCurrency Currency) const;
    UFUNCTION(BlueprintPure, Category="Progression") int32 GetTreeInvestment(const UBreakerProgressionTree* Tree) const;
    UFUNCTION(BlueprintPure, Category="Progression") const FBreakerProgressionState& GetProgressionState() const { return State; }
    UFUNCTION(BlueprintCallable, Category="Progression") void LoadProgressionState(const FBreakerProgressionState& NewState);

    // Every tree this character may spend in, fallback content included. The
    // UI enumerates trees here and nodes through UBreakerProgressionTree.
    UFUNCTION(BlueprintPure, Category="Progression") TArray<UBreakerProgressionTree*> GetAvailableTrees() const;
    // Public because it is one half of "does the front end describe the class I
    // am actually in" — the axis the owner caught broken in play and the axis
    // no attribute-value test can reach (RiorsEdge.Progression.ClassSwap*).
    UFUNCTION(BlueprintPure, Category="Progression") bool IsAbilityUnlocked(FName AbilityId) const;

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
    // Whole tree-layer damage contribution as a 1.0-based multiplier, node
    // effects and the point-spend baseline together. Read for display only:
    // combat reads the composed DamageMultiplier ATTRIBUTE, never this, or the
    // two layers would multiply instead of sharing one additive bucket.
    UFUNCTION(BlueprintPure, Category="Progression") float GetDamageMultiplier() const { return CachedStats.DamageMultiplier; }
    // Points committed to nodes across both wallets (rank x cost).
    UFUNCTION(BlueprintPure, Category="Progression") float GetSpentPoints() const;
    // The point-spend baseline in whole percent, before node effects.
    UFUNCTION(BlueprintPure, Category="Progression") float GetPointSpendDamagePercent() const;

    // O2 PLACEHOLDER, retuned under O27: 0.25% increased damage per committed
    // point, down from 1.0%.
    //
    // At 1.0% this contributed roughly +69% at a full point budget against
    // roughly +19% from every damage node combined, so HOW MANY points you had
    // spent mattered about 3.5x more than WHERE you spent them. O27 rules that
    // choices must beat accumulation, so the power moved into the nodes and this
    // dropped to a floor: it exists only so that a point committed to a purely
    // defensive or utility node is not literally zero offence. It cannot
    // differentiate two builds, because both of them spend every point.
    //
    // Still EditAnywhere on BP_BreakerCharacter, and still safe to set to 0,
    // which leaves node choices as the entire tree contribution.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Progression|Tuning", meta=(ClampMin="0.0", UIMax="5.0"))
    float IncreasedDamagePerSpentPoint = 0.25f;

    // O3's hard cap and Damage-Pipeline §4's per-multiplier ceiling, enforced
    // in AggregateStats. Public so the skill screen and the band test read the
    // same two numbers the fold does.
    static constexpr int32 MaxDamageMoreSources = 3;
    static constexpr float SingleMoreCeiling = 1.30f;

    // O39's implementation note: "ApplySliceDefaultsIfFresh's auto-lock to
    // Swift should be retired to a dev convenience once the class screen's
    // real path works, so the screen is actually exercised." This is that
    // gate. DEFAULT TRUE: today's flow, and every existing test that relies
    // on a fresh pawn arriving as Swift, is unchanged until this is
    // deliberately turned off (dev convenience or the class screen's own
    // config, once it exists).
    UPROPERTY(EditAnywhere, Category="Progression|Tuning")
    bool bAutoLockSwiftIfFresh = true;

    // Playtest hook: hands the gym the slice point budget so trees can be
    // exercised without an XP loop. O2 PLACEHOLDER budget (XP §9).
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Progression|Playtest")
    void GrantPlaytestPoints(int32 ClassPoints, int32 CorePoints);

    // ---- The XP loop (O40b) --------------------------------------------
    // Awards XP and re-derives the level. Returns the number of levels gained
    // so the caller can fire a level-up tell — a silent level-up is the
    // feel-first failure this project keeps finding.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Progression|XP")
    int32 AwardExperience(int32 Amount);

    // Convenience for the kill path: rank and area level in, XP awarded out.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Progression|XP")
    int32 AwardKillExperience(EBreakerMonsterRank Rank, int32 AreaLevel);

    UFUNCTION(BlueprintPure, Category="Progression|XP") int32 GetTotalExperience() const { return State.TotalExperience; }
    UFUNCTION(BlueprintPure, Category="Progression|XP") int32 GetCharacterLevel() const { return State.CharacterLevel; }
    UFUNCTION(BlueprintPure, Category="Progression|XP") float GetLevelProgressFraction() const;
    UFUNCTION(BlueprintPure, Category="Progression|XP") int32 GetXpToNextLevel() const;

    // The curve is EditAnywhere on the component so it can be retuned in the
    // editor with no recompile, matching how every other tunable in this
    // project is exposed. Every value inside it is O2 PLACEHOLDER.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Progression|XP") FBreakerExperienceCurve ExperienceCurve;

    // Raised when a level is actually gained, never on ordinary XP gain.
    UPROPERTY(BlueprintAssignable, Category="Progression|XP") FBreakerLevelGained OnLevelGained;

    // Re-derives CharacterLevel from TotalExperience. The ONLY writer of
    // CharacterLevel; called after an award and after a save load, so a
    // retuned curve takes effect on existing characters.
    void RefreshLevelFromXp();
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
    // The optional out-contribution is this layer's offer to the unified
    // application path in UBreakerAttributeSet, built from the same raw
    // buckets so Increased percentages reach the shared additive bucket
    // unmerged.
    // Conditional effects pay out only for conditions active in Conditions. The
    // default empty state means "standing still", so the skill screen's
    // projection and every pre-existing call site keep their exact behaviour.
    static FBreakerNodeStats AggregateStats(const TArray<const UBreakerProgressionNode*>& Nodes, const TArray<FBreakerNodeRank>& Ranks,
        FBreakerAttributeContribution* OutContribution = nullptr, const FBreakerBuildConditionState& Conditions = FBreakerBuildConditionState());

    // The movement/momentum conditions this component last folded in.
    const FBreakerBuildConditionState& GetActiveConditions() const { return ActiveConditions; }

    // Binds the attribute set this component contributes to. BeginPlay calls
    // it with the set found on the owner's ability system; tests call it with
    // a standalone set. Capturing the bases is the attribute set's job.
    void BindAttributes(UBreakerAttributeSet* InAttributes);

    // This layer's current offer, exactly as submitted.
    const FBreakerAttributeContribution& GetAttributeContribution() const { return CachedContribution; }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Progression") TObjectPtr<UBreakerClassDefinition> ClassDefinition;
    UPROPERTY(BlueprintAssignable, Category="Progression") FBreakerProgressionChanged OnProgressionChanged;

private:
    UPROPERTY(VisibleInstanceOnly, Category="Progression") FBreakerProgressionState State;
    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;

    FBreakerNodeStats CachedStats;
    // No base-value cache lives here any more. The attribute set owns the one
    // true base; this component only ever submits a contribution, which is why
    // skill nodes and gear now stack instead of overwriting each other.
    FBreakerAttributeContribution CachedContribution;
    FBreakerBuildConditionState ActiveConditions;
    // The node tags currently pushed onto the owner's ability system, so the
    // next submission can remove exactly what the last one added. Without this
    // a respec would leave keystone tags behind and the ultimate would keep a
    // rewrite the player no longer owns.
    FGameplayTagContainer PublishedNodeTags;
    // Audit item 6 (perf): running totals of points committed per currency,
    // maintained at purchase/respec/load instead of recomputed by walking
    // every owned rank's node definition on every RecalculateStats. See
    // GetRefundValue's comment for the complexity this replaced.
    int32 CachedSpentClassPoints = 0;
    int32 CachedSpentCorePoints = 0;

    // Conditional node effects are live state, so the offer they belong to has
    // to be rebuilt on a transition. Called from the tick; only recalculates
    // when the active set actually moved.
    void RefreshBuildConditions();
    int32 GetRefundValue(EBreakerPointCurrency Currency) const;
    const UBreakerProgressionNode* FindOwnedNodeDefinition(FName NodeId, EBreakerPointCurrency Currency) const;
    void CollectKnownNodes(TArray<const UBreakerProgressionNode*>& OutNodes, EBreakerPointCurrency Currency) const;
    TArray<FBreakerNodeRank>& RanksFor(EBreakerPointCurrency Currency);
    const TArray<FBreakerNodeRank>& RanksFor(EBreakerPointCurrency Currency) const;
    int32& SpentPointsFor(EBreakerPointCurrency Currency);
    // Rebuilds both running totals from State's current rank arrays. O(ranks x
    // N^2) like the per-call path it replaces on the hot path, but this is
    // called only when ranks are bulk-replaced (LoadProgressionState) rather
    // than on every RecalculateStats.
    void RecomputeSpentPointsFromState();
    void RecalculateStats();
    void ApplyStatsToAttributes();
    // Mirrors CachedStats.GrantedTags onto the owner's ability system as loose
    // tags. Rule-rewrite nodes published tags that only this component could
    // see, so a keystone whose whole job is to rewrite an ability — Overdrive
    // resolves its variant from the OWNER's tag container — could not reach it.
    void PublishNodeTagsToAbilitySystem();
};
