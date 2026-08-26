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
struct FBreakerRiftDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBreakerProgressionChanged);
// NewLevel, and how many levels arrived at once — a single kill can cross more
// than one level early on, and a tell that says "level 2" when the player
// reached 4 is worse than no tell.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBreakerLevelGained, int32, NewLevel, int32, LevelsGained);

// ---------------------------------------------------------------------------
// STAGE 6 — one target-conditional Increased line, published for the target
// side to resolve (Hook-And-Condition-Vocabulary §3.2).
// ---------------------------------------------------------------------------
// A node effect whose requirement names a Target* condition cannot be composed
// at the source: the outgoing pass has no target (H2's structural hole), a
// projectile has no target at fire time, and the one call site that knows both
// actors is UBreakerCombatComponent::ReceiveDamage (H3). So the progression
// component publishes these effects as ROWS — the full requirement, the stat
// target and the rank-scaled percent — alongside its ordinary attribute
// contribution, and ReceiveDamage adds the percent of every satisfied row into
// the SAME additive Increased bucket via the request's source split
// (FBreakerDamageRequest::SourceIncreasedPercent). Never a multiplier:
// target-side lines are Increased-bucket only, by the doc's own rule.
struct RIORSEDGE_API FBreakerTargetConditionRider
{
    EBreakerBuildCondition Condition = EBreakerBuildCondition::Always;
    TArray<EBreakerBuildCondition> AlsoRequires;
    // Recorded so a future partition consumer (AbilityDamage etc.) can route
    // rows without a table migration; ReceiveDamage consumes Damage rows only
    // today and the builder is loud about anything else.
    EBreakerNodeStatTarget StatTarget = EBreakerNodeStatTarget::Damage;
    // Whole percent, already multiplied by the owned rank.
    float Percent = 0.0f;
    // O141: the HIT-TIME More half. Zero on every Increased row; on the one
    // target-gated More row (Collapse) it is the authored percent above 1.0
    // (30.0 == x1.30), NEVER rank-scaled — a More does not scale with rank
    // anywhere. Paid at the combat site by multiplying the request's standing
    // More product under the one O34 ceiling: headroom, never a slot, so the
    // strongest-three sort never sees it and "N / 3 MORE" keeps meaning slot
    // competition. At most one such row may exist in authored content —
    // TreeContent pins the population — because a single x1.30 rider already
    // saturates an ability build to 98.5% of the ceiling and a second would
    // read as a line that does not work.
    float MorePercent = 0.0f;
    // O98: set on a rider-delivered slice row (MeleeDamage today, keyed on
    // Damage.Melee). The row pays only when the request's SourceTags carry
    // this tag — the slice is selected by what the hit says it IS, never by
    // what triggered it. Invalid on every pool-targeted row, and an invalid
    // tag requires nothing.
    FGameplayTag RequiredSourceTag;
};

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

    // O168's third commit — the rift completion payout. Bound in BeginPlay to
    // ABreakerGameMode::OnRiftCompleted (GROUND's published seam; the event
    // fires ONLY on completion, in-world, at the latch, structurally once per
    // run) and public so the payout is testable by direct call with no world.
    // Grants XP here and Riftglass through the owner's equipment component —
    // both travel-surviving state by construction, which is what firing at
    // completion rather than exit was ruled on. Ignores a broadcast for a
    // pawn that is not this owner: the event carries WHO completed, and a
    // second player's rift is not this character's payday.
    void HandleRiftCompleted(const FBreakerRiftDefinition& Rift, APawn* Player);

    // ORDERS ruling 1: the node id of Swift's enhanced-dash passive, seeded
    // at rank 1 wherever a character becomes (or loads as) Swift — the class's
    // second free verb, a node and never a slot occupant. Public so tests and
    // the board can name it without a string copy drifting.
    static const FName SwiftGrantedDashNodeId;

    // Every tree this character may spend in, fallback content included. The
    // UI enumerates trees here and nodes through UBreakerProgressionTree.
    UFUNCTION(BlueprintPure, Category="Progression") TArray<UBreakerProgressionTree*> GetAvailableTrees() const;
    // Public because it is one half of "does the front end describe the class I
    // am actually in" — the axis the owner caught broken in play and the axis
    // no attribute-value test can reach (RiorsEdge.Progression.ClassSwap*).
    UFUNCTION(BlueprintPure, Category="Progression") bool IsAbilityUnlocked(FName AbilityId) const;

    // ---- O100: ability acquisition ---------------------------------------
    // What this character can still buy, what it costs, and the one way to buy
    // it. The quartermaster screen is a reader of these three and authors no
    // rule of its own — the same division the ability component keeps with
    // IsAbilityUnlocked, and for the same reason: two copies of an unlock rule
    // drift.
    UFUNCTION(BlueprintPure, Category="Progression") TArray<FName> GetUnlockableAbilityIds() const;
    UFUNCTION(BlueprintPure, Category="Progression") int32 GetUnspentAbilityTokens() const { return State.UnspentAbilityTokens; }
    // Refuses on: no class, no tokens, an id this class does not offer as
    // unlockable, and an id already unlocked. A REFUSED SPEND COSTS NOTHING —
    // the same rule as a refused craft, and the same failure class if it is
    // broken: one refusal becomes a lost-currency report nobody can reproduce.
    UFUNCTION(BlueprintCallable, Category="Progression") bool SpendAbilityToken(FName AbilityId, FText& OutFailureReason);
    // Pays the level entitlement's token half. Called from
    // GrantLevelPointEntitlement so the two can never disagree about level.
    void GrantAbilityTokens();

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

    // Static tree-legality check (owner ruling 2026-08-16): a node with
    // MaxRank > 1 may NEVER author a MorePercent effect. Rank does not scale
    // a More — a rank-2 x1.25 would be x1.5625, which no node table means —
    // so a multi-rank More node is authored nonsense whichever way the
    // aggregator resolves it. AggregateStats already refuses to scale the
    // value by rank; this makes the refusal a red test instead of a silent
    // repricing. Pure and static so the content suite can scan every
    // registered tree with no actor; OutReason names the offending node for
    // the test log.
    static bool IsNodeMoreAuthoringLegal(const UBreakerProgressionNode* Node, FString* OutReason = nullptr);

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
    void GrantPlaytestPoints(int32 DoctrinePoints, int32 CorePoints);

    // ---- The fifteen world Core Points (O7) -----------------------------
    // The other half of a character's sixty-five, and until this existed the
    // only things that ever moved UnspentCorePoints were the level entitlement
    // and the playtest hook above — so fifteen canon points were unreachable
    // and the eight campaign missions paying one had nothing to pay with.
    //
    // IDEMPOTENCE IS NOT KEPT HERE. It is a quest flag, because a world point
    // is one-time and permanent (design rule 1) and the flag set is already
    // presence-only, monotonic, per-character and saved on the change — which
    // is that requirement exactly, built and tested. A second bespoke "already
    // claimed" set would be a second source of truth for the same fact, and
    // the one that does not survive a reload is always the new one.
    //
    // Returns true only when the point was actually granted, so a caller can
    // fire a tell without having to ask twice.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Progression|WorldPoints")
    bool GrantWorldPoint(FName SourceId, class UBreakerQuestJournal* Journal);

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
    // XP-And-Pacing §4, the missing half of the XP loop: pays the difference
    // between what the current level entitles the character to
    // (1 Class Point/level to 30, 1 Core Point/level to 50) and what has
    // already been paid, into the unspent pools. Monotonic — never claws
    // back after a downward curve retune. Called after every real level gain
    // and once on load, so a save from before the entitlement existed is
    // brought current the first time it opens.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Progression|XP")
    void GrantLevelPointEntitlement();
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

    // ---- STAGE 6: the target-conditional rider table ---------------------
    // Pure over the same node/rank inputs AggregateStats takes, for the same
    // reason: testable with no actor. Emits one row per owned rank of every
    // effect whose requirement needs target state, IncreasedPercent bucket and
    // Damage stat target only — a target-conditional MorePercent is
    // warn-and-dropped exactly like the other unpaid Mores (it would need the
    // strongest-three More selection re-run per event per target, which is
    // both expensive and unexplainable), and any other bucket or stat target
    // is warn-and-dropped until a lane exists to pay it.
    static TArray<FBreakerTargetConditionRider> BuildTargetConditionRiders(
        const TArray<const UBreakerProgressionNode*>& Nodes, const TArray<FBreakerNodeRank>& Ranks);

    // The rows this build currently publishes, re-derived in RecalculateStats
    // with everything else — purchases, respecs, loads and condition
    // transitions all re-state the current truth, so nothing new needs
    // invalidating. Read by UBreakerCombatComponent::ReceiveDamage off
    // Request.Instigator.
    const TArray<FBreakerTargetConditionRider>& GetTargetConditionRiders() const { return CachedTargetRiders; }

    // Binds the attribute set this component contributes to. BeginPlay calls
    // it with the set found on the owner's ability system; tests call it with
    // a standalone set. Capturing the bases is the attribute set's job.
    void BindAttributes(UBreakerAttributeSet* InAttributes);

    // This layer's current offer, exactly as submitted.
    const FBreakerAttributeContribution& GetAttributeContribution() const { return CachedContribution; }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Progression") TObjectPtr<UBreakerClassDefinition> ClassDefinition;
    UPROPERTY(BlueprintAssignable, Category="Progression") FBreakerProgressionChanged OnProgressionChanged;

private:
    // ORDERS ruling 1's grant mechanism: writes rank 1 of the granted node
    // into the doctrine ranks when the character is Swift and does not hold
    // it. Idempotent, cost 0 (so refunds and spent-point totals are untouched
    // by construction), called from every path that sets or restores a class
    // — the three Choose/DevForce paths, LoadProgressionState (a migrated or
    // roster-written Swift save carries no rank), and the doctrine respec
    // (whose clear would otherwise take a grant a refund never paid for).
    void SeedGrantedNodes();

    // Ruled in the blocked-questions pass (Part One-U item 20, ahead of any
    // node deletion): a loaded rank row whose id no longer resolves is
    // DROPPED AND CREDITED to its currency's wallet at the same fallback
    // cost the spent recompute charges (1 per rank) — exactly undoing what
    // that charge took. Before this, a removed node silently taxed every
    // save that had bought it, permanently: charged at fallback, granted
    // nothing, invisible. Load-time only, loud per row, and skipped (loudly)
    // when no class definition is resolvable — a sweep that cannot resolve
    // anything must not read "wipe everything to the wallet".
    void DropUnknownRanksAndCredit();

    UPROPERTY(VisibleInstanceOnly, Category="Progression") FBreakerProgressionState State;
    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;

    FBreakerNodeStats CachedStats;
    // No base-value cache lives here any more. The attribute set owns the one
    // true base; this component only ever submits a contribution, which is why
    // skill nodes and gear now stack instead of overwriting each other.
    FBreakerAttributeContribution CachedContribution;
    // Stage 6: the published target-conditional rows. See GetTargetConditionRiders.
    TArray<FBreakerTargetConditionRider> CachedTargetRiders;
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
    int32 CachedSpentDoctrinePoints = 0;

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
    // THE THREE-WAY SEAM. Every currency switch in this class routes through
    // one of these, so a fourth pool -- if one is ever ruled -- is three
    // functions rather than eight ternaries scattered across the file. The
    // retired class arm is kept live rather than removed: a v5 save being
    // migrated still has ranks in it when the migration reads them.
    int32& WalletFor(EBreakerPointCurrency Currency);
    int32 WalletFor(EBreakerPointCurrency Currency) const;
    // Rebuilds both running totals from State's current rank arrays. O(ranks x
    // N^2) like the per-call path it replaces on the hot path, but this is
    // called only when ranks are bulk-replaced (LoadProgressionState) rather
    // than on every RecalculateStats.
    void RecomputeSpentPointsFromState();
    void RecalculateStats();
    void ApplyStatsToAttributes();
    // The ClassResourceDecay bridge: delivers the composed decay multiplier to
    // the owner's class resource component as a keyed loop override (the
    // PushLoopOverride seam). Called from RecalculateStats so purchases,
    // respecs, loads AND condition transitions all re-state the current truth.
    void PushLoopValveOverrides();
    // Mirrors CachedStats.GrantedTags onto the owner's ability system as loose
    // tags. Rule-rewrite nodes published tags that only this component could
    // see, so a keystone whose whole job is to rewrite an ability — Overdrive
    // resolves its variant from the OWNER's tag container — could not reach it.
    void PublishNodeTagsToAbilitySystem();
};
