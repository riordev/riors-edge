#include "Progression/BreakerProgressionComponent.h"

#include "Game/BreakerGameMode.h"
#include "Game/BreakerRiftDefinition.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerRiftRewardMath.h"

#include "Attributes/BreakerAttributeAggregation.h"

// The per-source More ceiling exists in two headers for include-cycle reasons;
// this is the guard that keeps them one value (O3/O34: a restated constant
// that can drift is the bug class the aggregator header warns about).
static_assert(UBreakerProgressionComponent::SingleMoreCeiling == FBreakerAttributeAggregator::SingleMoreCeiling,
    "Progression's SingleMoreCeiling must equal the aggregator's (O34: one More budget)");

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Classes/BreakerGritComponent.h"
#include "Progression/BreakerWorldPoints.h"
#include "Save/BreakerQuestJournal.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Game/BreakerGameInstance.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#define LOCTEXT_NAMESPACE "BreakerProgression"

UBreakerProgressionComponent::UBreakerProgressionComponent()
{
    // Ticks only to notice movement-state transitions for conditional node
    // effects. The tick itself is a byte comparison; the recalculation behind
    // it runs on a transition, not on a frame.
    PrimaryComponentTick.bCanEverTick = true;
}

void UBreakerProgressionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    RefreshBuildConditions();
}

void UBreakerProgressionComponent::RefreshBuildConditions()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    const FBreakerBuildConditionState Evaluated = FBreakerBuildConditionState::EvaluateForActor(GetOwner());
    if (Evaluated == ActiveConditions) return;
    ActiveConditions = Evaluated;
    RecalculateStats();
}

void UBreakerProgressionComponent::BeginPlay()
{
    Super::BeginPlay();
    UBreakerAttributeSet* FoundAttributes = nullptr;
    if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner()))
    {
        if (UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent())
        {
            FoundAttributes = const_cast<UBreakerAttributeSet*>(ASC->GetSet<UBreakerAttributeSet>());
        }
    }
    // Seed the slice budget before any save load runs. Nothing else called
    // this, which is why a gym pawn reached the tree screen with zero points;
    // LoadProgressionState re-runs it after restoring a save.
    ApplySliceDefaultsIfFresh();
    BindAttributes(FoundAttributes);
    // O168's third commit: bind the rift completion payout to GROUND's seam.
    // Authority only (the game mode exists only there), weak so a component
    // torn down before its world cannot be called into. The handler itself
    // filters by pawn, so every player component may bind the one event.
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (ABreakerGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ABreakerGameMode>() : nullptr)
        {
            Mode->OnRiftCompleted.AddWeakLambda(this,
                [this](const FBreakerRiftDefinition& Rift, APawn* Player) { HandleRiftCompleted(Rift, Player); });
        }
    }
}

void UBreakerProgressionComponent::BindAttributes(UBreakerAttributeSet* InAttributes)
{
    Attributes = InAttributes;
    // No base cache here. The attribute set owns the one true base and
    // capturing is idempotent, so it does not matter whether progression or
    // equipment binds first — neither can snapshot a base that already
    // contains the other's contribution. That was the bug.
    if (Attributes) Attributes->CaptureAttributeBases();
    RecalculateStats();
}

const FName UBreakerProgressionComponent::SwiftGrantedDashNodeId(TEXT("Swift.Kinetic.Longstride"));

void UBreakerProgressionComponent::SeedGrantedNodes()
{
    // See the declaration. Swift-only today; a second class earning a granted
    // node adds a row here, not a mechanism.
    if (State.PermanentClass != EBreakerClassId::Swift) return;
    for (const FBreakerNodeRank& Rank : State.DoctrineNodeRanks)
    {
        if (Rank.NodeId == SwiftGrantedDashNodeId) return;
    }
    State.DoctrineNodeRanks.Add({SwiftGrantedDashNodeId, 1});
}

void UBreakerProgressionComponent::HandleRiftCompleted(const FBreakerRiftDefinition& Rift, APawn* Player)
{
    // The event carries WHO completed; a broadcast for another pawn is not
    // this character's payday. Null owner or no authority cannot happen on
    // the bound path (the bind is authority-gated) but the direct-call test
    // path deserves the same guards as every other grant.
    if (!GetOwner() || GetOwner() != Player) return;

    const int32 AreaLevel = Rift.EffectiveAreaLevel();
    const int32 Xp = BreakerRiftReward::XpForCompletion(AreaLevel);
    const int32 Riftglass = BreakerRiftReward::RiftglassForCompletion(AreaLevel);

    AwardExperience(Xp);
    // Riftglass lands on the account-wide wallet through the owner's
    // equipment component — the same GrantForgeCurrency every reward path
    // uses. A pawn with no equipment component (a bare rig) forfeits nothing
    // silently: say so, because a payout that vanished is a lost-currency
    // report nobody can reproduce.
    if (UBreakerEquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UBreakerEquipmentComponent>())
    {
        Equipment->GrantForgeCurrency(Riftglass);
        UE_LOG(LogTemp, Display, TEXT("[BreakerProgression] rift complete (%s, AL %d): +%d XP, +%d Riftglass (O168/O137)."),
            *Rift.AreaName.ToString(), AreaLevel, Xp, Riftglass);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[BreakerProgression] rift complete (AL %d): +%d XP granted, but no equipment component holds the wallet — %d Riftglass NOT paid."),
            AreaLevel, Xp, Riftglass);
    }
}

void UBreakerProgressionComponent::DevForceClass(EBreakerClassId ClassId)
{
    State.PermanentClass = ClassId;
    // THE STALE-DEFINITION GAP IS CLOSED, and it was not cosmetic. Owner
    // playtest: "when selecting a different class i can only see the swift
    // nodes", and "i dont see proper ability selection based on what character
    // im at". Both were this one line. A dev swap kept whatever ClassDefinition
    // was already held, and TWO readers trust it directly:
    // GetAvailableTrees unions ClassDefinition->BranchTrees (so a Caster was
    // handed Swift's three branch trees) and IsAbilityUnlocked answers from
    // ClassDefinition->StarterAbilityIds/BaseUltimateId (so a Caster was
    // offered Swift's abilities). RecalculateStats already guarded itself by
    // re-reading State.PermanentClass, which is why the STATS were right while
    // the whole front end was wrong — the bug was invisible to every test that
    // checked numbers.
    //
    // The recorded objection was that re-fetching unconditionally would stomp
    // an authored Data Asset with the C++ fallback. That objection is answered
    // by re-fetching ONLY on a mismatch: a definition describing a DIFFERENT
    // class is never the right one to keep, whatever its source, and a
    // correctly-matching authored asset is left exactly where it is.
    if (!ClassDefinition || ClassDefinition->ClassId != ClassId)
    {
        if (UBreakerClassDefinition* Fallback = UBreakerProgressionLibrary::GetFallbackClassDefinition(ClassId))
        {
            ClassDefinition = Fallback;
        }
        else
        {
            // O39: the class has no implemented kit. Dropping the stale
            // definition is still correct — showing the PREVIOUS class's trees
            // and abilities is worse than showing none — but it must be loud,
            // because an empty class screen is otherwise indistinguishable
            // from a broken one.
            ClassDefinition = nullptr;
            UE_LOG(LogTemp, Warning,
                TEXT("DevForceClass(%d): no implemented kit, so the class definition is cleared rather than left ")
                TEXT("describing the class just left. Trees and abilities will be empty for this class (O39)."),
                static_cast<int32>(ClassId));
        }
    }
    SeedGrantedNodes();
    RecalculateStats();
    OnProgressionChanged.Broadcast();
}

bool UBreakerProgressionComponent::ChoosePermanentClassById(EBreakerClassId ClassId)
{
    if (ClassId == EBreakerClassId::None || State.PermanentClass != EBreakerClassId::None) return false;
    // O39: a permanent lock into a class with no implemented kit is a trap
    // (null definition, no trees, no abilities, no resource) — refuse it here,
    // not only in the UI, so console/Blueprint/save paths get the same rule.
    // DevForceClass remains the dev-mode way to inhabit an unbuilt class.
    //
    // D12 FIX: the gate is PER-CLASS-ID. The old form read
    // `!ClassDefinition && !GetFallbackClassDefinition(ClassId)`, so any
    // assigned Blueprint-default ClassDefinition — describing whatever class
    // it happened to describe — made the gate pass for EVERY ClassId. A held
    // definition only vouches for the class it actually describes.
    const bool bHeldDefinitionMatches = ClassDefinition && ClassDefinition->ClassId == ClassId;
    if (!bHeldDefinitionMatches && !UBreakerProgressionLibrary::GetFallbackClassDefinition(ClassId))
    {
        UE_LOG(LogTemp, Warning, TEXT("ChoosePermanentClassById refused %d: no implemented kit (O39)"), static_cast<int32>(ClassId));
        return false;
    }
    State.PermanentClass = ClassId;
    if (!bHeldDefinitionMatches)
    {
        ClassDefinition = UBreakerProgressionLibrary::GetFallbackClassDefinition(ClassId);
    }
    // D11 FIX: seed the loadout exactly as ChoosePermanentClass does. Without
    // this a fresh character's loadout stayed all-None — the HUD fired the
    // DEFAULT table's abilities while the picker showed nothing selected, two
    // different answers to "what am I holding".
    if (ClassDefinition)
    {
        if (ClassDefinition->StarterAbilityIds.Num() > 0) State.AbilityLoadout.ClassAbilityOne = ClassDefinition->StarterAbilityIds[0];
        if (ClassDefinition->StarterAbilityIds.Num() > 1) State.AbilityLoadout.ClassAbilityTwo = ClassDefinition->StarterAbilityIds[1];
        State.AbilityLoadout.Ultimate = ClassDefinition->BaseUltimateId;
    }
    SeedGrantedNodes();
    RecalculateStats();
    // Locking a class is a progression change like any other. Without this the
    // class-resource loops never learn they went live: UBreakerMomentumComponent
    // caches bIsSwift in HandleProgressionChanged, which runs once at BeginPlay
    // and then only on this event, so a character who picks Swift mid-session
    // had dead Momentum until some unrelated purchase happened to broadcast.
    OnProgressionChanged.Broadcast();
    return true;
}

bool UBreakerProgressionComponent::ChoosePermanentClass(const UBreakerClassDefinition* NewClassDefinition)
{
    if (!NewClassDefinition || NewClassDefinition->ClassId == EBreakerClassId::None || State.PermanentClass != EBreakerClassId::None)
    {
        return false;
    }

    ClassDefinition = const_cast<UBreakerClassDefinition*>(NewClassDefinition);
    State.PermanentClass = NewClassDefinition->ClassId;
    if (NewClassDefinition->StarterAbilityIds.Num() > 0) State.AbilityLoadout.ClassAbilityOne = NewClassDefinition->StarterAbilityIds[0];
    if (NewClassDefinition->StarterAbilityIds.Num() > 1) State.AbilityLoadout.ClassAbilityTwo = NewClassDefinition->StarterAbilityIds[1];
    State.AbilityLoadout.Ultimate = NewClassDefinition->BaseUltimateId;
    SeedGrantedNodes();
    RecalculateStats();
    // Same reason as ChoosePermanentClassById: this is the Data-Asset-driven
    // twin of that path and the listeners cannot tell them apart.
    OnProgressionChanged.Broadcast();
    return true;
}

bool UBreakerProgressionComponent::CanPurchaseNode(const UBreakerProgressionTree* Tree, FName NodeId, FText& OutFailureReason) const
{
    if (!Tree)
    {
        OutFailureReason = LOCTEXT("MissingTree", "Progression tree is missing.");
        return false;
    }
    const UBreakerProgressionNode* Node = Tree->FindNode(NodeId);
    if (!Node || Node->Currency != Tree->Currency)
    {
        OutFailureReason = LOCTEXT("InvalidNode", "That node does not belong to this tree.");
        return false;
    }
    if (Node->RequiredClass != EBreakerClassId::None && Node->RequiredClass != State.PermanentClass)
    {
        OutFailureReason = LOCTEXT("WrongClass", "This node belongs to another class.");
        return false;
    }
    const int32 CurrentRank = GetNodeRank(NodeId, Node->Currency);
    if (CurrentRank >= Node->MaxRank)
    {
        OutFailureReason = LOCTEXT("MaxRank", "This node is already at maximum rank.");
        return false;
    }
    for (const FBreakerNodePrerequisite& Prerequisite : Node->Prerequisites)
    {
        if (GetNodeRank(Prerequisite.NodeId, Node->Currency) < Prerequisite.RequiredRank)
        {
            OutFailureReason = LOCTEXT("Prerequisite", "A prerequisite node has not been met.");
            return false;
        }
    }
    // THE RING (owner ruling, Phase 4): in a tree that carries a connectivity
    // graph, a node is bought FROM the graph — it is an entry, or it touches
    // a node already owned. This layers UNDER the AND Prerequisites above:
    // adjacency is how you reach a wheel, prerequisites are what it charges
    // once you are there. Trees with no edges (every doctrine) skip this
    // entirely, bit-identically to before the ring existed.
    if (Tree->AdjacencyEdges.Num() > 0 && !Tree->EntryNodeIds.Contains(NodeId))
    {
        bool bConnected = false;
        for (const FBreakerNodeEdge& Edge : Tree->AdjacencyEdges)
        {
            const FName Neighbor = Edge.A == NodeId ? Edge.B : (Edge.B == NodeId ? Edge.A : FName(NAME_None));
            if (!Neighbor.IsNone() && GetNodeRank(Neighbor, Node->Currency) >= 1)
            {
                bConnected = true;
                break;
            }
        }
        if (!bConnected)
        {
            OutFailureReason = LOCTEXT("RingUnreached", "Reach this node through the ring first: no connected node is owned.");
            return false;
        }
    }
    for (const FName ExcludedId : Node->MutuallyExclusiveNodeIds)
    {
        if (GetNodeRank(ExcludedId, Node->Currency) > 0)
        {
            OutFailureReason = LOCTEXT("Exclusive", "This node conflicts with an existing choice.");
            return false;
        }
    }
    // O37: a branch keystone/cornerstone node is purchasable only once the
    // character has committed to ITS branch. Ordinary nodes of every branch
    // stay freely purchasable — O15 is untouched — so this is the one place
    // O37's "commitment empowers rather than excludes" is actually enforced.
    if (Node->bCornerstone && State.CommittedBranch != Tree->TreeId)
    {
        OutFailureReason = LOCTEXT("NotCommitted", "Commit to this branch before its keystone can be purchased.");
        return false;
    }
    const int32 Gate = FMath::Max(Node->RequiredTreeInvestment, Node->bCornerstone ? Tree->CornerstoneInvestmentGate : 0);
    if (GetTreeInvestment(Tree) < Gate)
    {
        OutFailureReason = LOCTEXT("InvestmentGate", "More points must be invested in this tree first.");
        return false;
    }
    if (GetUnspentPoints(Node->Currency) < Node->CostPerRank)
    {
        OutFailureReason = LOCTEXT("NoPoints", "Not enough points are available.");
        return false;
    }

    OutFailureReason = FText::GetEmpty();
    return true;
}

bool UBreakerProgressionComponent::PurchaseNode(const UBreakerProgressionTree* Tree, FName NodeId, FText& OutFailureReason)
{
    if (!CanPurchaseNode(Tree, NodeId, OutFailureReason)) return false;

    const UBreakerProgressionNode* Node = Tree->FindNode(NodeId);
    TArray<FBreakerNodeRank>& Ranks = RanksFor(Node->Currency);
    FBreakerNodeRank* Existing = Ranks.FindByPredicate([NodeId](const FBreakerNodeRank& Rank) { return Rank.NodeId == NodeId; });
    if (Existing) ++Existing->Rank;
    else Ranks.Add({NodeId, 1});

    int32& AvailablePoints = WalletFor(Node->Currency);
    AvailablePoints -= Node->CostPerRank;
    // Running total maintained here instead of recomputed by walking node
    // definitions inside GetSpentPoints/GetRefundValue on every
    // RecalculateStats (audit item 6 perf fix).
    SpentPointsFor(Node->Currency) += Node->CostPerRank;

    RecalculateStats();
    OnProgressionChanged.Broadcast();
    return true;
}

bool UBreakerProgressionComponent::EquipAbility(EBreakerAbilitySlot Slot, FName AbilityId, FText& OutFailureReason)
{
    if (AbilityId.IsNone() || !IsAbilityUnlocked(AbilityId))
    {
        OutFailureReason = LOCTEXT("AbilityLocked", "That ability has not been unlocked.");
        return false;
    }
    if (State.AbilityLoadout.Contains(AbilityId))
    {
        OutFailureReason = LOCTEXT("AbilityDuplicate", "That ability is already equipped.");
        return false;
    }

    if (Slot == EBreakerAbilitySlot::Ultimate)
    {
        if (!ClassDefinition || AbilityId != ClassDefinition->BaseUltimateId)
        {
            OutFailureReason = LOCTEXT("NotUltimate", "Only an unlocked class ultimate can use the ultimate slot.");
            return false;
        }
        State.AbilityLoadout.Ultimate = AbilityId;
    }
    else if (Slot == EBreakerAbilitySlot::ClassAbilityOne) State.AbilityLoadout.ClassAbilityOne = AbilityId;
    else State.AbilityLoadout.ClassAbilityTwo = AbilityId;

    OutFailureReason = FText::GetEmpty();
    OnProgressionChanged.Broadcast();
    return true;
}

bool UBreakerProgressionComponent::RespecAtForge(EBreakerPointCurrency Currency, bool bIsAtForge, FText& OutFailureReason)
{
    if (!bIsAtForge)
    {
        OutFailureReason = LOCTEXT("ForgeRequired", "Respecs are only available at a Forge.");
        return false;
    }

    TArray<FBreakerNodeRank>& Ranks = RanksFor(Currency);
    const int32 Refunded = GetRefundValue(Currency);
    Ranks.Reset();
    // The running total this currency was tracking is refunded in full and
    // starts back at zero (audit item 6).
    SpentPointsFor(Currency) = 0;
    // O111: A DOCTRINE RESPEC IS A REFUND, AND IT USED NOT TO BE. While the
    // pool arrived whole at commitment, zeroing was right: the eight belonged
    // to the commitment this call clears, and refunding them would have left a
    // character holding points with no doctrine to spend them in while
    // respec-then-recommit minted eight more.
    //
    // The points are now EARNED, two at a time at four benchmarks, and they are
    // the character's rather than the commitment's. Zeroing them would delete
    // progression a player was paid for reaching level 40 -- and because
    // commitment no longer pays, nothing would ever give them back. The mint
    // this used to guard against is closed at the other end now: the grant is a
    // function of level settled against a counter, so there is no event to
    // repeat.
    if (Currency == EBreakerPointCurrency::DoctrinePoints) State.UnspentDoctrinePoints += Refunded;
    else if (Currency == EBreakerPointCurrency::ClassPoints_Retired) State.UnspentClassPoints += Refunded;
    else State.UnspentCorePoints += Refunded;
    // The commitment clear moved off the retired currency with the nodes it
    // governs. Left where it was it would be unreachable -- nothing calls a
    // respec of a pool nothing spends -- and a committed character could never
    // un-commit, sealing the Forge's one escape hatch.
    if (Currency == EBreakerPointCurrency::DoctrinePoints)
    {
        State.AbilityLoadout.ClassAbilityOne = ClassDefinition && ClassDefinition->StarterAbilityIds.Num() > 0
            ? ClassDefinition->StarterAbilityIds[0] : NAME_None;
        State.AbilityLoadout.ClassAbilityTwo = ClassDefinition && ClassDefinition->StarterAbilityIds.Num() > 1
            ? ClassDefinition->StarterAbilityIds[1] : NAME_None;
        State.AbilityLoadout.Ultimate = ClassDefinition ? ClassDefinition->BaseUltimateId : NAME_None;
        // O37: commitment is a class-branch choice, so the respec that clears
        // branch node ranks (including any keystone) clears the commitment
        // with them. This is the one-way rule's escape hatch: "no
        // un-committing without a Forge visit", not "never".
        State.CommittedBranch = NAME_None;
        // AND NOT THE UNLOCKS. Everything else this function touches is
        // refunded in full — ranks, points, the loadout, the commitment — so
        // the reasonable default reading is that abilities go back too. They do
        // not: an unlock is bought with a token that a respec does not return,
        // and taking the ability while keeping the token spent would be a
        // refund that costs the player something. UnlockedAbilityIds,
        // UnspentAbilityTokens and AbilityTokensGranted are deliberately absent
        // from this function; RiorsEdge.Progression.AbilityUnlocks.
        // SurvivesRespec is what keeps them absent.
    }
    // A respec refunds what was PAID; the granted rank was never paid for
    // (cost 0, seeded), so the clear above must not be how a Swift loses its
    // class verb. Re-seed before the recalculation.
    SeedGrantedNodes();
    // Cleared ranks must clear their effects, tags and verb grants included.
    RecalculateStats();
    OnProgressionChanged.Broadcast();
    OutFailureReason = FText::GetEmpty();
    return true;
}

bool UBreakerProgressionComponent::CommitToBranch(FName BranchTreeId, FText& OutFailureReason)
{
    // ONE-WAY: refuses an overwrite rather than silently replacing it. A
    // Forge respec (ClassPoints) is the only path back to None.
    if (!State.CommittedBranch.IsNone())
    {
        OutFailureReason = LOCTEXT("AlreadyCommitted", "A subclass commitment is already made; only a Forge respec can change it.");
        return false;
    }
    if (BranchTreeId.IsNone())
    {
        OutFailureReason = LOCTEXT("NoBranchNamed", "No branch was named.");
        return false;
    }
    // The named tree must actually be a CLASS BRANCH this character can spend
    // in — not Core (RequiredClass == None), which is not a subclass and has
    // no keystone tier gated by commitment. A typo'd or foreign id would
    // otherwise silently lock the player out of every keystone forever.
    bool bFound = false;
    for (const UBreakerProgressionTree* Tree : GetAvailableTrees())
    {
        if (Tree && Tree->TreeId == BranchTreeId && Tree->RequiredClass == State.PermanentClass
            && Tree->RequiredClass != EBreakerClassId::None)
        {
            bFound = true;
            break;
        }
    }
    if (!bFound)
    {
        OutFailureReason = LOCTEXT("UnknownBranch", "That branch is not available to this character.");
        return false;
    }
    State.CommittedBranch = BranchTreeId;
    // O111: THE EIGHT ARRIVE HERE, WHOLE. Commitment is the grant event, so a
    // character carries no doctrine points until they have a doctrine to spend
    // them in -- which is also why a doctrine respec zeroes the wallet rather
    // than refunding it.
    //
    // COMMITTING PAYS NOTHING NOW, and that is the ruling rather than an
    // oversight. The pool used to arrive whole at this line; it now arrives two
    // points at a time at four benchmarks, so commitment chooses WHERE the
    // points go and the benchmarks decide WHEN they exist.
    //
    // That also closes a hole this line needed a paragraph to defend. While
    // commitment paid, the only thing standing between a player and unlimited
    // points was that a respec zeroed the wallet before re-committing; the
    // grant is no longer an event anything can repeat, so there is nothing to
    // farm and nothing to zero.
    OutFailureReason = FText::GetEmpty();
    OnProgressionChanged.Broadcast();
    return true;
}

int32 UBreakerProgressionComponent::GetNodeRank(FName NodeId, EBreakerPointCurrency Currency) const
{
    const FBreakerNodeRank* Found = RanksFor(Currency).FindByPredicate([NodeId](const FBreakerNodeRank& Rank) { return Rank.NodeId == NodeId; });
    return Found ? Found->Rank : 0;
}

int32 UBreakerProgressionComponent::GetUnspentPoints(EBreakerPointCurrency Currency) const
{
    return WalletFor(Currency);
}

void UBreakerProgressionComponent::LoadProgressionState(const FBreakerProgressionState& NewState)
{
    // Permanent class can be loaded but never replaced by ordinary class
    // selection. Save migration/validation will be centralized later.
    State = NewState;
    // Refetch on MISMATCH, not only on null — the same D-fix DevForceClass
    // already carries, for the same two readers. BeginPlay's fresh-pawn
    // seeding can lock Swift (and hold Swift's definition) BEFORE the save
    // loads; loading a Caster save then replaced State but kept the Swift
    // definition, so GetAvailableTrees and IsAbilityUnlocked — and the
    // loadout seeding just below — all answered for the wrong class. A
    // correctly-matching authored Data Asset is left exactly where it is.
    if (State.PermanentClass != EBreakerClassId::None
        && (!ClassDefinition || ClassDefinition->ClassId != State.PermanentClass))
    {
        ClassDefinition = UBreakerProgressionLibrary::GetFallbackClassDefinition(State.PermanentClass);
    }
    // D11, the roster path: a character created from the create screen is
    // written with a class and an all-None loadout (the roster writes the save
    // directly, no Choose* runs). Seed the starters here so a fresh character
    // shows its selection instead of firing the default table with an empty
    // picker. A save with ANY equipped id is left exactly as it was.
    if (ClassDefinition && State.PermanentClass != EBreakerClassId::None
        && State.AbilityLoadout.ClassAbilityOne.IsNone()
        && State.AbilityLoadout.ClassAbilityTwo.IsNone()
        && State.AbilityLoadout.Ultimate.IsNone())
    {
        if (ClassDefinition->StarterAbilityIds.Num() > 0) State.AbilityLoadout.ClassAbilityOne = ClassDefinition->StarterAbilityIds[0];
        if (ClassDefinition->StarterAbilityIds.Num() > 1) State.AbilityLoadout.ClassAbilityTwo = ClassDefinition->StarterAbilityIds[1];
        State.AbilityLoadout.Ultimate = ClassDefinition->BaseUltimateId;
    }
    // A migrated or roster-written Swift save carries no granted rank; a
    // save that has one is left untouched. Before the spent rebuild so the
    // totals see the final ranks (the granted rank is cost 0 either way).
    SeedGrantedNodes();
    // Ranks were just bulk-replaced from outside; the running spent-points
    // totals have to be rebuilt from what actually loaded rather than
    // incrementally tracked, exactly once, here (audit item 6).
    RecomputeSpentPointsFromState();
    RecalculateStats();
    // A save written before slice seeding existed restores an empty economy.
    // Re-seed here so loading never leaves the tree screen unspendable; the
    // freshness test means a save with any real progress is left untouched.
    ApplySliceDefaultsIfFresh();
    // LEGACY REPAIR for saves from before the per-level entitlement existed:
    // they received the slice lump (or are receiving it just above) but carry
    // granted-counters of zero, and paying min(Level, cap) on top of the lump
    // would double-pay levels 1-10. A non-fresh state with zero counters can
    // only be such a save — the lump seeds the counters on every path that
    // grants it — so seed them here the same way before settling up.
    if (State.LevelClassPointsGranted == 0 && State.LevelCorePointsGranted == 0)
    {
        State.LevelClassPointsGranted = UBreakerProgressionLibrary::SliceClassPointGrant;
        State.LevelCorePointsGranted = UBreakerProgressionLibrary::SliceCorePointGrant;
    }
    RefreshLevelFromXp();
    GrantLevelPointEntitlement();
    OnProgressionChanged.Broadcast();
}

TArray<UBreakerProgressionTree*> UBreakerProgressionComponent::GetAvailableTrees() const
{
    TArray<UBreakerProgressionTree*> Trees;
    if (ClassDefinition)
    {
        for (const TObjectPtr<UBreakerProgressionTree>& Tree : ClassDefinition->BranchTrees)
        {
            // The SAME class filter RecalculateStats applies, and for the same
            // reason: this union is the one that showed a Caster Swift's three
            // branch trees. DevForceClass no longer leaves a mismatched
            // definition behind, so this is defence in depth rather than the
            // fix — but the reader should not depend on every writer being
            // correct, and an authored Data Asset can list a foreign tree by
            // simple authoring error with nothing to catch it.
            if (!Tree) continue;
            if (Tree->RequiredClass != EBreakerClassId::None && Tree->RequiredClass != State.PermanentClass) continue;
            Trees.AddUnique(Tree.Get());
        }
    }
    for (UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetTreesForClass(State.PermanentClass))
    {
        if (Tree) Trees.AddUnique(Tree);
    }
    return Trees;
}

void UBreakerProgressionComponent::RefreshLevelFromXp()
{
    // The one and only writer of CharacterLevel. Everything else reads it.
    State.CharacterLevel = UBreakerExperienceLibrary::LevelForTotalXp(State.TotalExperience, ExperienceCurve);
}

int32 UBreakerProgressionComponent::AwardExperience(int32 Amount)
{
    if (Amount <= 0) return 0;
    if (GetOwner() && !GetOwner()->HasAuthority()) return 0;

    const int32 Before = State.CharacterLevel;
    // Clamped at the total the cap costs rather than accumulating forever. The
    // level cap is a HARD stop with no post-cap power (a locked decision), so
    // banking XP past it would be storing a quantity nothing can ever spend —
    // and would silently un-cap the day someone raised MaxCharacterLevel,
    // handing every existing character a fistful of free levels.
    const int32 Ceiling = UBreakerExperienceLibrary::TotalXpToReachLevel(
        UBreakerExperienceLibrary::MaxCharacterLevel, ExperienceCurve);
    State.TotalExperience = FMath::Min(State.TotalExperience + Amount, Ceiling);
    RefreshLevelFromXp();

    const int32 Gained = State.CharacterLevel - Before;
    if (Gained > 0)
    {
        // THE POINT PAYS BEFORE THE EVENT FIRES. The level-up used to grant
        // nothing — a HUD flash over an unchanged pool — which made the whole
        // XP loop decorative (the tree economy was the one-time slice lump).
        // Paying first means anything listening to OnLevelGained (the HUD's
        // flash, a future "point available" tell) observes the pool it will
        // tell the player about.
        GrantLevelPointEntitlement();
        // A level-up that changes numbers and says nothing is the failure mode
        // this project keeps rediscovering, so the event is raised here rather
        // than left for a caller to remember.
        OnLevelGained.Broadcast(State.CharacterLevel, Gained);
        OnProgressionChanged.Broadcast();
    }
    return Gained;
}

void UBreakerProgressionComponent::GrantLevelPointEntitlement()
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    // O100's tokens ride the same trigger as the point entitlement and use the
    // same cumulative shape, so a character who gains three levels at once, or
    // whose level moves under a curve retune, is paid exactly once for each.
    GrantAbilityTokens();
    // The entitlement is a FUNCTION OF LEVEL, not an event: min(Level, cap)
    // points per currency, minus what has already been paid. Event-shaped
    // grants ("+1 on each level-up") cannot survive the rederived-level rule —
    // a curve retune that jumps a character three levels would need to
    // remember how many events it owes, which is exactly this counter.
    // O111: NO LEVEL PAYS A CLASS POINT. UnspentClassPoints and
    // LevelClassPointsGranted survive as save fields because they are
    // serialized; the v5 -> v6 step zeroes them and nothing writes them again.
    const int32 CoreEntitled = FMath::Min(State.CharacterLevel, UBreakerProgressionLibrary::CorePointCapLevel);
    const int32 CoreOwed = CoreEntitled - State.LevelCorePointsGranted;
    bool bPaid = false;
    if (CoreOwed > 0)
    {
        State.UnspentCorePoints += CoreOwed;
        State.LevelCorePointsGranted = CoreEntitled;
        bPaid = true;
    }

    // O111's doctrine points, on the same cumulative shape and for the same
    // reason. Paid whether or not a doctrine is committed: the points are the
    // character's, the commitment only decides which board can spend them, and
    // withholding them until commitment would make the benchmark a thing that
    // silently did nothing for a player who had not chosen yet.
    //
    // THE ONE ORDERING TRAP HERE. This runs before the doctrine is necessarily
    // chosen, so a character can hold doctrine points with no committed branch.
    // That is correct and is what the Forge screen shows; what must never
    // happen is the reverse -- a committed character who was paid twice -- and
    // the counter is what rules that out, not this ordering.
    const int32 DoctrineEntitled = UBreakerProgressionLibrary::DoctrinePointEntitlement(State.CharacterLevel);
    const int32 DoctrineOwed = DoctrineEntitled - State.LevelDoctrinePointsGranted;
    if (DoctrineOwed > 0)
    {
        State.UnspentDoctrinePoints += DoctrineOwed;
        State.LevelDoctrinePointsGranted = DoctrineEntitled;
        bPaid = true;
    }

    if (!bPaid) return;
    OnProgressionChanged.Broadcast();
}

int32 UBreakerProgressionComponent::AwardKillExperience(EBreakerMonsterRank Rank, int32 AreaLevel)
{
    const int32 Xp = UBreakerExperienceLibrary::XpForKill(Rank, AreaLevel, ExperienceCurve);
    AwardExperience(Xp);
    return Xp;
}

float UBreakerProgressionComponent::GetLevelProgressFraction() const
{
    return UBreakerExperienceLibrary::LevelProgressFraction(State.TotalExperience, ExperienceCurve);
}

int32 UBreakerProgressionComponent::GetXpToNextLevel() const
{
    return UBreakerExperienceLibrary::XpToNextLevel(State.CharacterLevel, ExperienceCurve);
}

bool UBreakerProgressionComponent::GrantWorldPoint(FName SourceId, UBreakerQuestJournal* Journal)
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return false;
    if (!Journal) return false;

    // An unknown id is refused rather than paid. The fifteen are canon (O7) and
    // a typo that silently granted a sixteenth point would inflate the budget
    // the whole Core Tree is sized against — 65 is the number two constellations
    // and a bit were validated at, and it is not a number to lose by accident.
    if (!UBreakerWorldPointLibrary::IsKnownSource(SourceId)) return false;

    const FName Flag = UBreakerWorldPointLibrary::FlagForSource(SourceId);
    if (Journal->HasFlag(Flag)) return false;   // already claimed; one-time and permanent

    Journal->SetFlag(Flag);
    State.UnspentCorePoints = FMath::Max(0, State.UnspentCorePoints + 1);
    OnProgressionChanged.Broadcast();
    return true;
}

void UBreakerProgressionComponent::GrantPlaytestPoints(int32 DoctrinePoints, int32 CorePoints)
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    // O111: THE FIRST ARGUMENT FOLLOWED ITS NODES. It granted Class Points to
    // buy class-tree nodes; those trees are doctrines now, so it grants
    // Doctrine Points to buy the same nodes. The retired wallet is never
    // credited again -- a dev grant into a pool nothing spends is a rig that
    // reports success and buys nothing.
    State.UnspentDoctrinePoints = FMath::Max(0, State.UnspentDoctrinePoints + DoctrinePoints);
    State.UnspentCorePoints = FMath::Max(0, State.UnspentCorePoints + CorePoints);
    OnProgressionChanged.Broadcast();
}

void UBreakerProgressionComponent::ApplySliceDefaultsIfFresh()
{
    // "Fresh" is a statement about the point economy, not about the save file.
    // A character with nothing allocated in either currency and an empty pool
    // has never received the slice budget — whether it is a brand-new gym pawn
    // or an existing save written before this seeding existed. A permanent
    // class already being chosen no longer disqualifies it; only real spending
    // or a non-empty pool does. That is what left owner saves stuck on
    // "CLASS 0 | CORE 0 UNSPENT" with a locked class and nothing to spend.
    // The two LIVE pools. It also tested the retired rank array, which is
    // correct only while the migration has already zeroed it -- the reasoning
    // ProgressionTypes.h rules out at that field's own declaration. The retired
    // WALLET stays, because a v5 payload can still carry a non-zero one.
    // The GRANTED rank does not count against freshness: it is seeded, not
    // spent (cost 0, SeedGrantedNodes), so a Swift whose only doctrine rank
    // is the grant has exactly the empty economy this repair exists for —
    // counting it would quietly withhold the slice lump from every seeded
    // character loaded from a rankless save.
    int32 SpentDoctrineRanks = 0;
    for (const FBreakerNodeRank& Rank : State.DoctrineNodeRanks)
    {
        if (Rank.NodeId != SwiftGrantedDashNodeId) ++SpentDoctrineRanks;
    }
    const bool bFresh = State.CoreNodeRanks.Num() == 0 && SpentDoctrineRanks == 0
        && State.UnspentClassPoints == 0 && State.UnspentCorePoints == 0
        && State.UnspentDoctrinePoints == 0;
    if (!bFresh) return;
    // Only pick a class for a character that has none; a chosen class is
    // kept. Gated on bAutoLockSwiftIfFresh (O39): default true keeps this
    // line's behaviour identical to before the flag existed.
    //
    // AND gated on the session NOT being driven by a roster character (O39's
    // retirement note, now that the real class path works). A pawn whose
    // session names an ActiveCharacterId is about to load — or has just
    // loaded — that character's save, and that save is the ONLY authority on
    // its class. Auto-locking Swift in the gap between BeginPlay and the load
    // is how a created Caster arrived wearing Swift. Dev flows keep their
    // Swift default: a capture run, a PIE drop-in and every test rig have no
    // ActiveCharacterId (or no game instance at all) and are unchanged.
    const UBreakerGameInstance* Session = GetOwner() ? GetOwner()->GetGameInstance<UBreakerGameInstance>() : nullptr;
    const bool bRosterCharacterSession = Session && Session->ActiveCharacterId.IsValid();
    if (bAutoLockSwiftIfFresh && !bRosterCharacterSession && State.PermanentClass == EBreakerClassId::None) ChoosePermanentClassById(EBreakerClassId::Swift);
    // O2 PLACEHOLDER: 10 Class / 12 Core is the XP-And-Pacing §9 slice budget
    // at cap 10; the shipping numbers come from the curve Data Asset.
    GrantPlaytestPoints(UBreakerProgressionLibrary::SliceClassPointGrant, UBreakerProgressionLibrary::SliceCorePointGrant);
    // THE LUMP IS AN ADVANCE on the per-level entitlement, not a bonus beside
    // it. Seeding the granted-counters to the lump's own values means levels
    // 1-10 pay nothing extra (pre-paid above) and level 11 pays the 11th
    // Class Point — which is also the level a branch keystone first becomes
    // affordable through play (investment gate 8 + cost 3). Without this
    // seed, per-level grants would stack on the lump and a level-10 character
    // would hold 20 Class Points against a documented budget of 10.
    State.LevelClassPointsGranted = FMath::Max(State.LevelClassPointsGranted, UBreakerProgressionLibrary::SliceClassPointGrant);
    State.LevelCorePointsGranted = FMath::Max(State.LevelCorePointsGranted, UBreakerProgressionLibrary::SliceCorePointGrant);
}

int32 UBreakerProgressionComponent::GetTreeInvestment(const UBreakerProgressionTree* Tree) const
{
    int32 Total = 0;
    if (!Tree) return Total;
    for (const UBreakerProgressionNode* Node : Tree->Nodes)
    {
        if (Node) Total += GetNodeRank(Node->NodeId, Tree->Currency) * Node->CostPerRank;
    }
    return Total;
}

int32 UBreakerProgressionComponent::GetRefundValue(EBreakerPointCurrency Currency) const
{
    // O(1): a running total maintained at purchase/respec/load (audit item 6)
    // instead of recomputed here by walking every owned rank's node
    // definition. That walk used to call FindOwnedNodeDefinition once PER
    // OWNED RANK, and FindOwnedNodeDefinition rebuilt the ENTIRE known-node
    // array from scratch on every single call (CollectKnownNodes, itself
    // O(N^2) via TArray::AddUnique) — O(ranks x N^2) on GetSpentPoints's path,
    // which RecalculateStats calls on every movement-state transition.
    switch (Currency)
    {
    case EBreakerPointCurrency::ClassPoints_Retired: return CachedSpentClassPoints;
    case EBreakerPointCurrency::DoctrinePoints:      return CachedSpentDoctrinePoints;
    default:                                         return CachedSpentCorePoints;
    }
}

void UBreakerProgressionComponent::RecomputeSpentPointsFromState()
{
    // Called once per load rather than once per RecalculateStats, so the
    // O(ranks x N^2) walk FindOwnedNodeDefinition does is an acceptable
    // one-time cost here — the fix is keeping that walk OFF the per-tick
    // path, not eliminating it everywhere it could ever run.
    auto SumFor = [this](EBreakerPointCurrency Currency)
    {
        int32 Total = 0;
        for (const FBreakerNodeRank& Rank : RanksFor(Currency))
        {
            const UBreakerProgressionNode* Definition = FindOwnedNodeDefinition(Rank.NodeId, Currency);
            Total += Rank.Rank * (Definition ? Definition->CostPerRank : 1);
        }
        return Total;
    };
    // Two live pools. The retired one has no rank storage left to sum.
    CachedSpentClassPoints = 0;
    CachedSpentDoctrinePoints = SumFor(EBreakerPointCurrency::DoctrinePoints);
    CachedSpentCorePoints = SumFor(EBreakerPointCurrency::CorePoints);
}

int32& UBreakerProgressionComponent::SpentPointsFor(EBreakerPointCurrency Currency)
{
    switch (Currency)
    {
    case EBreakerPointCurrency::ClassPoints_Retired: return CachedSpentClassPoints;
    case EBreakerPointCurrency::DoctrinePoints:      return CachedSpentDoctrinePoints;
    default:                                         return CachedSpentCorePoints;
    }
}

int32& UBreakerProgressionComponent::WalletFor(EBreakerPointCurrency Currency)
{
    switch (Currency)
    {
    case EBreakerPointCurrency::ClassPoints_Retired: return State.UnspentClassPoints;
    case EBreakerPointCurrency::DoctrinePoints:      return State.UnspentDoctrinePoints;
    default:                                         return State.UnspentCorePoints;
    }
}

int32 UBreakerProgressionComponent::WalletFor(EBreakerPointCurrency Currency) const
{
    switch (Currency)
    {
    case EBreakerPointCurrency::ClassPoints_Retired: return State.UnspentClassPoints;
    case EBreakerPointCurrency::DoctrinePoints:      return State.UnspentDoctrinePoints;
    default:                                         return State.UnspentCorePoints;
    }
}

void UBreakerProgressionComponent::CollectKnownNodes(TArray<const UBreakerProgressionNode*>& OutNodes, EBreakerPointCurrency Currency) const
{
    auto AddTree = [this, &OutNodes, Currency](const UBreakerProgressionTree* Tree)
    {
        if (!Tree || Tree->Currency != Currency) return;
        // O37/audit item 6 class filter: a tree scoped to another class must
        // not keep paying out ranks a dev class swap left behind.
        // RequiredClass == None (Core) is class-agnostic and always
        // included; CanPurchaseNode already refuses a NEW purchase outside
        // this rule (the WrongClass check above) — this is the same rule
        // applied at AGGREGATION, so an EXISTING rank stops contributing the
        // moment the permanent class changes instead of only blocking future
        // spending. Correctly covers DevForceClass's documented gap too: it
        // deliberately leaves a stale ClassDefinition in place, so this reads
        // State.PermanentClass rather than trusting ClassDefinition->BranchTrees
        // to already be scoped to the current class.
        if (Tree->RequiredClass != EBreakerClassId::None && Tree->RequiredClass != State.PermanentClass) return;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (Node) OutNodes.AddUnique(Node);
        }
    };

    if (ClassDefinition)
    {
        for (const TObjectPtr<UBreakerProgressionTree>& Tree : ClassDefinition->BranchTrees) AddTree(Tree.Get());
    }
    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees()) AddTree(Tree);
}

const UBreakerProgressionNode* UBreakerProgressionComponent::FindOwnedNodeDefinition(FName NodeId, EBreakerPointCurrency Currency) const
{
    TArray<const UBreakerProgressionNode*> Nodes;
    CollectKnownNodes(Nodes, Currency);
    for (const UBreakerProgressionNode* Node : Nodes)
    {
        if (Node->NodeId == NodeId) return Node;
    }
    return nullptr;
}

TArray<FName> UBreakerProgressionComponent::GetUnlockableAbilityIds() const
{
    return ClassDefinition ? ClassDefinition->UnlockableAbilityIds : TArray<FName>();
}

bool UBreakerProgressionComponent::SpendAbilityToken(FName AbilityId, FText& OutFailureReason)
{
    OutFailureReason = FText::GetEmpty();
    if (!ClassDefinition || State.PermanentClass == EBreakerClassId::None)
    {
        OutFailureReason = LOCTEXT("UnlockNoClass", "Lock a class before unlocking abilities.");
        return false;
    }
    // Not an ability this class sells. Checked before the token count so the
    // player is told the real reason rather than "you have no tokens" for an id
    // no number of tokens could ever buy.
    if (!ClassDefinition->UnlockableAbilityIds.Contains(AbilityId))
    {
        OutFailureReason = LOCTEXT("UnlockNotOffered", "The quartermaster does not carry that.");
        return false;
    }
    if (IsAbilityUnlocked(AbilityId))
    {
        OutFailureReason = LOCTEXT("UnlockAlreadyOwned", "Already unlocked.");
        return false;
    }
    if (State.UnspentAbilityTokens <= 0)
    {
        OutFailureReason = LOCTEXT("UnlockNoTokens", "No unlock tokens.");
        return false;
    }

    // EVERY REFUSAL ABOVE RETURNS BEFORE THIS POINT, so a refused spend has
    // debited nothing. Partial spends are how one refusal becomes a
    // lost-currency bug report, and the Forge's refused-craft rule is the same
    // rule for the same reason.
    --State.UnspentAbilityTokens;
    State.UnlockedAbilityIds.AddUnique(AbilityId);
    OnProgressionChanged.Broadcast();
    return true;
}

void UBreakerProgressionComponent::GrantAbilityTokens()
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    if (!ClassDefinition) return;
    const int32 Entitled = UBreakerProgressionLibrary::AbilityTokenEntitlement(
        State.CharacterLevel, ClassDefinition->UnlockableAbilityIds.Num());
    const int32 Owed = Entitled - State.AbilityTokensGranted;
    if (Owed <= 0) return;
    State.UnspentAbilityTokens += Owed;
    State.AbilityTokensGranted = Entitled;
}

bool UBreakerProgressionComponent::IsAbilityUnlocked(FName AbilityId) const
{
    // The definition is only authoritative for the class it actually describes.
    // The same defence GetAvailableTrees applies, and against the same failure:
    // a definition left over from another class made the ABILITIES tab offer
    // that class's kit (owner: "i dont see proper ability selection based on
    // what character im at"). DevForceClass no longer leaves one behind, so
    // this guard should never fire — which is exactly why it is cheap to keep.
    const bool bDefinitionDescribesCurrentClass =
        ClassDefinition && (State.PermanentClass == EBreakerClassId::None || ClassDefinition->ClassId == State.PermanentClass);
    // O100: free at level one is the STARTERS and the ultimate. Everything else
    // this class offers is bought, one token at a time, and lives in the
    // character's own unlocked set — so two characters of the same class can
    // hold different kits, which is the whole point of the system.
    if (bDefinitionDescribesCurrentClass
        && (ClassDefinition->BaseUltimateId == AbilityId || ClassDefinition->StarterAbilityIds.Contains(AbilityId))) return true;
    if (State.UnlockedAbilityIds.Contains(AbilityId)) return true;
    for (const EBreakerPointCurrency Currency : {EBreakerPointCurrency::CorePoints, EBreakerPointCurrency::DoctrinePoints})
    {
        TArray<const UBreakerProgressionNode*> Nodes;
        CollectKnownNodes(Nodes, Currency);
        for (const UBreakerProgressionNode* Node : Nodes)
        {
            if (GetNodeRank(Node->NodeId, Currency) > 0 && Node->GrantedAbilityIds.Contains(AbilityId)) return true;
        }
    }
    return false;
}

FBreakerNodeStats UBreakerProgressionComponent::AggregateStats(const TArray<const UBreakerProgressionNode*>& Nodes, const TArray<FBreakerNodeRank>& Ranks,
    FBreakerAttributeContribution* OutContribution, const FBreakerBuildConditionState& Conditions)
{
    constexpr int32 TargetCount = static_cast<int32>(EBreakerNodeStatTarget::Count);
    float FlatByTarget[TargetCount] = {};
    float IncreasedByTarget[TargetCount] = {};
    // One entry per owned More source, kept separate until the O3 cap has been
    // applied. Sorting and truncating a list is the only honest way to enforce
    // "a build may hold at most three": composing first and clamping the
    // product afterwards would silently reprice the nodes the player chose.
    //
    // A4 (owner ruling 2026-08-16): the DoT More lane now exists — VW12 / Long
    // Dark's DamageOverTime-targeted MorePercent composes instead of being
    // warn-and-dropped. Both lanes share the ONE O34 budget, so the selection
    // runs over Damage and DamageOverTime sources TOGETHER: strongest three
    // across both, each clamped at the per-source ceiling, and each selected
    // source lands in its own lane's product (a DoT More multiplies ticks only,
    // through ComposeDotSourcePower; it never inflates a direct hit).
    //
    // O54 widened the same selection again. There are now three More lanes —
    // the weapon-delivered pool, the ability-delivered pool and DoT ticks —
    // plus SHARED sources, which multiply both delivery lanes from one
    // purchase. O74 is what keeps this honest: the lanes share ONE budget of
    // three, so a shared source spends one slot and not two, and adding a lane
    // never adds headroom.
    enum class EBreakerMoreLane : uint8 { Weapon, Ability, Shared, Dot };
    struct FBreakerMoreSource { float Multiplier = 1.0f; EBreakerMoreLane Lane = EBreakerMoreLane::Weapon; };
    TArray<FBreakerMoreSource> MoreSources;
    float ActiveConditionalPercent = 0.0f;
    float PotentialConditionalPercent = 0.0f;

    FBreakerNodeStats Stats;
    for (const FBreakerNodeRank& Rank : Ranks)
    {
        if (Rank.Rank <= 0) continue;
        const UBreakerProgressionNode* const* Found = Nodes.FindByPredicate(
            [&Rank](const UBreakerProgressionNode* Node) { return Node && Node->NodeId == Rank.NodeId; });
        if (!Found) continue;
        const UBreakerProgressionNode* Node = *Found;

        const int32 EffectiveRank = FMath::Min(Rank.Rank, Node->MaxRank);
        for (const FBreakerNodeEffect& Effect : Node->Effects)
        {
            const int32 Target = static_cast<int32>(Effect.StatTarget);
            if (Target < 0 || Target >= TargetCount) continue;
            const float Value = Effect.ValuePerRank * static_cast<float>(EffectiveRank);

            const bool bConditional = Effect.Condition != EBreakerBuildCondition::Always;
            // STAGE 6: an effect that needs the TARGET's state is a RIDER —
            // published by BuildTargetConditionRiders and composed in
            // UBreakerCombatComponent::ReceiveDamage, the one site that knows
            // both actors. When this state carries no target half (the live
            // per-actor state never does), skip it WITHOUT asking
            // SatisfiesAll: the effect is no longer dead, it is resolved at
            // the other end of the pipeline, and the warn-once path must not
            // call a working lane a wiring bug. A state that DOES carry
            // target info — All()'s tooltip hypothetical, a test fixture —
            // still composes it right here, which is what keeps the
            // "potential" display an upper bound rather than a blind spot.
            //
            // The MorePercent case is skipped under ANY state — including
            // All() — because since O141 the rider path pays it AT HIT TIME
            // (headroom under the one ceiling, never a slot): composing it
            // here too would double-pay it, it must never enter the
            // strongest-three sort below, and All()'s tooltip hypothetical
            // must not promise the UNCLAMPED value a saturated build will
            // never receive. The one legal shape and the loud drop for every
            // other live in BuildTargetConditionRiders.
            if (Effect.RequiresTargetState())
            {
                if (Effect.StatBucket == EBreakerNodeStatBucket::MorePercent) continue;
                if (!Conditions.HasTargetState())
                {
                    if (bConditional && BreakerIsDeliveredDamagePool(Effect.StatTarget)
                        && Effect.StatBucket == EBreakerNodeStatBucket::IncreasedPercent)
                    {
                        PotentialConditionalPercent += Value;
                    }
                    continue;
                }
            }
            // COMPOSITION (owner ruling, 2026-08-15: "conditions do compose
            // yes"). An effect may now require several conditions at once, and
            // SatisfiesAll is also where an unsatisfiable requirement becomes
            // LOUD — a condition nothing can currently evaluate warns once
            // rather than silently never paying, which is the failure mode that
            // produced the third jump and the phantom keystones.
            const bool bLive = Conditions.SatisfiesAll(Effect.Condition, Effect.AlsoRequires);
            if (bConditional && BreakerIsDeliveredDamagePool(Effect.StatTarget)
                && Effect.StatBucket == EBreakerNodeStatBucket::IncreasedPercent)
            {
                PotentialConditionalPercent += Value;
                if (bLive) ActiveConditionalPercent += Value;
            }
            // A conditional effect whose state is false contributes nothing at
            // all, in any bucket.
            if (!bLive) continue;

            if (Effect.StatBucket == EBreakerNodeStatBucket::Flat) FlatByTarget[Target] += Value;
            else if (Effect.StatBucket == EBreakerNodeStatBucket::IncreasedPercent) IncreasedByTarget[Target] += Value;
            else if (BreakerDamagePoolFor(Effect.StatTarget) != EBreakerDamagePool::None)
            {
                // Rank does NOT scale a More multiplier — a rank-2 x1.25 would
                // be x1.5625, which no node table means. Every More node in the
                // content is single-rank; IsNodeMoreAuthoringLegal is the
                // validator that keeps it so, and this Min against rank 1 is
                // the belt-and-braces guard behind it.
                // A4 (owner ruling 2026-08-16): DamageOverTime joined Damage
                // as a composable More lane. Long Dark was what it was built
                // for and O95 removed that multiplier; see the lane itself.
                // O54: and the two delivery lanes joined it, with the shared
                // pool as a fourth kind that multiplies both of them from one
                // source. Still ONE budget of three (O74) — see the selection.
                EBreakerMoreLane Lane = EBreakerMoreLane::Weapon;
                switch (BreakerDamagePoolFor(Effect.StatTarget))
                {
                case EBreakerDamagePool::Weapon:         Lane = EBreakerMoreLane::Weapon;  break;
                case EBreakerDamagePool::Ability:        Lane = EBreakerMoreLane::Ability; break;
                case EBreakerDamagePool::Shared:         Lane = EBreakerMoreLane::Shared;  break;
                case EBreakerDamagePool::DamageOverTime: Lane = EBreakerMoreLane::Dot;     break;
                default: break;
                }
                MoreSources.Add({ 1.0f + FMath::Max(0.0f, Effect.ValuePerRank) / 100.0f, Lane });
            }
            else
            {
                // Audit item 2: every OTHER target used to drop a MorePercent
                // effect with no signal at all. Damage and (since A4)
                // DamageOverTime compose a More product; any other target
                // still needs its own aggregation lane before it can pay out.
                // Loud once per offending node rather than silent forever.
                static TSet<FName> WarnedOnceNodeIds;
                if (!WarnedOnceNodeIds.Contains(Node->NodeId))
                {
                    WarnedOnceNodeIds.Add(Node->NodeId);
                    UE_LOG(LogTemp, Warning,
                        TEXT("[BreakerProgression] node '%s' authors a MorePercent effect on stat target %d, but only the damage pools (Damage, WeaponDamage, AbilityDamage, DamageOverTime) compose a More product — this effect is silently dropped."),
                        *Node->NodeId.ToString(), Target);
                }
            }
        }
        Stats.GrantedTags.AppendTags(Node->GrantedTags);
    }

    // O3's hard cap of three, and Damage-Pipeline §4's per-multiplier ceiling of
    // 1.30x. The strongest three win — across BOTH lanes, one budget (A4/O34) —
    // so a fourth purchase is dead weight the player can be told about rather
    // than a quiet nerf to the other three. The source count reports every held
    // source, DoT Mores included: the skill screen's "N / 3 MORE" is the whole
    // budget, not the direct-hit lane alone.
    Stats.DamageMoreSourceCount = MoreSources.Num();
    // THE SORT IS BY RAW MAGNITUDE, NOT BY CONTRIBUTION. A Shared source
    // multiplies both delivery lanes from its one slot while a Weapon-only
    // source multiplies one — so a x1.20 Shared can be worth more to a build
    // than a x1.22 Weapon-only and still lose its slot to it here. Every More
    // in the content today is same-lane (AddDamageMore -> Shared), so the two
    // orderings agree; the first time a doctrine or an Anomalous item authors
    // a lane-specific More alongside Shared ones, this line is where the
    // selection stops meaning "most valuable three" and a decision is due.
    MoreSources.Sort([](const FBreakerMoreSource& A, const FBreakerMoreSource& B) { return A.Multiplier > B.Multiplier; });
    // O54/O74: four lanes out of ONE selection. A shared source is one slot of
    // the three and multiplies both delivery lanes; it is not two purchases and
    // it does not widen the budget. Splitting the budget per lane here would be
    // the per-lane ceiling O74 exists to delete.
    float WeaponMoreProduct = 1.0f;
    float AbilityMoreProduct = 1.0f;
    float DotMoreProduct = 1.0f;
    for (int32 Index = 0; Index < FMath::Min(MoreSources.Num(), MaxDamageMoreSources); ++Index)
    {
        const float Clamped = FMath::Min(MoreSources[Index].Multiplier, SingleMoreCeiling);
        switch (MoreSources[Index].Lane)
        {
        case EBreakerMoreLane::Weapon:  WeaponMoreProduct *= Clamped; break;
        case EBreakerMoreLane::Ability: AbilityMoreProduct *= Clamped; break;
        case EBreakerMoreLane::Dot:     DotMoreProduct *= Clamped; break;
        case EBreakerMoreLane::Shared:
            WeaponMoreProduct *= Clamped;
            AbilityMoreProduct *= Clamped;
            break;
        }
    }
    // The display figure keeps meaning the weapon lane, which is what it meant
    // before the split and what the skill screen's damage readout is measured
    // against.
    Stats.DamageMoreMultiplier = WeaponMoreProduct;
    Stats.AbilityDamageMoreMultiplier = AbilityMoreProduct;
    Stats.ActiveConditionalDamagePercent = ActiveConditionalPercent;
    Stats.PotentialConditionalDamagePercent = PotentialConditionalPercent;

    auto Flat = [&FlatByTarget](EBreakerNodeStatTarget Target) { return FlatByTarget[static_cast<int32>(Target)]; };
    auto Increased = [&IncreasedByTarget](EBreakerNodeStatTarget Target)
    {
        return 1.0f + IncreasedByTarget[static_cast<int32>(Target)] / 100.0f;
    };

    Stats.BonusHealth = Flat(EBreakerNodeStatTarget::Health);
    // CRIT IS A BOUNDED FLAT SIDE-CHANNEL BY DESIGN — documented as intended
    // (owner ruling 2026-08-16). Both crit lanes are Flat-bucket only: nodes
    // and gear bid flat points of chance and flat points of multiplier, there
    // is no Increased-percent lane for either, and none is missing. Crit's
    // ceiling is therefore the SUM of what the content authors, not a
    // percentage stack that scales with everything else — it cannot be
    // Increased-scaled into a mandatory stat, and an IncreasedPercent effect
    // authored against CriticalChance/CriticalDamage is dropped by the bucket
    // dispatch above exactly like any other laneless line. Do not "fix" this
    // by adding an Increased lane; the bound is the ruling.
    Stats.CriticalChanceBonus = Flat(EBreakerNodeStatTarget::CriticalChance) / 100.0f;
    Stats.CriticalMultiplierBonus = Flat(EBreakerNodeStatTarget::CriticalDamage) / 100.0f;
    Stats.DodgeChanceBonus = Flat(EBreakerNodeStatTarget::DodgeChance) / 100.0f;
    Stats.BlockChanceBonus = Flat(EBreakerNodeStatTarget::BlockChance) / 100.0f;
    Stats.MoveSpeedMultiplier = Increased(EBreakerNodeStatTarget::MoveSpeed);
    Stats.SlideSpeedMultiplier = Increased(EBreakerNodeStatTarget::SlideSpeed);
    Stats.AirControlMultiplier = Increased(EBreakerNodeStatTarget::AirControl);
    // The DashDistance lane (ORDERS ruling 1): single-bidder, read by
    // TryDash before the momentum hard cap. See the register in
    // BreakerProgressionTypes.h for the gear-migration note.
    Stats.DashDistanceMultiplier = Increased(EBreakerNodeStatTarget::DashDistance);
    Stats.DamageOverTimeMultiplier = Increased(EBreakerNodeStatTarget::DamageOverTime);
    // O54: the display figures for each delivery lane. Each is the shared pool
    // plus its own narrow line — the same sum the contribution bids, so the
    // card and the attribute can never print different numbers.
    Stats.DamageMultiplier = 1.0f + (IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::Damage)]
        + IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::WeaponDamage)]) / 100.0f;
    Stats.AbilityDamageMultiplier = 1.0f + (IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::Damage)]
        + IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::AbilityDamage)]) / 100.0f;
    // ---- Swift projectile channels (owner ruling 2026-08-16) --------------
    // Flat lanes only, on purpose: "+50% projectiles" is meaningless on a
    // single-shot weapon (the enum comment on ProjectileCount says so), and
    // pierce/chain/ricochet are whole mechanics, not percentages — an
    // IncreasedPercent authored against any of these four is dropped by the
    // bucket dispatch above exactly as it always was. These do not join the
    // attribute contribution below: no EBreakerAggregatedAttribute exists for
    // them and gear does not bid, so UBreakerWeaponComponent::GetShotChannels
    // reads this struct directly (see the lane-register comment in
    // BreakerProgressionTypes.h for the single-bidder reasoning).
    Stats.BonusProjectileCount = Flat(EBreakerNodeStatTarget::ProjectileCount);
    Stats.BonusPierceCount = Flat(EBreakerNodeStatTarget::Pierce);
    Stats.BonusChainCount = Flat(EBreakerNodeStatTarget::ChainCount);
    Stats.BonusRicochetCount = Flat(EBreakerNodeStatTarget::RicochetCount);
    // ---- Loop valve and ability geometry (2026-08-16) ---------------------
    // Four more single-bidder lanes: no aggregated attribute exists for any of
    // them and gear does not bid, so consumers read this struct directly (the
    // projectile-channel precedent above). All four are one additive Increased
    // bucket composed to a 1.0-based multiplier.
    //
    // ClassResourceDecay's sign convention, stated where the composition
    // happens: POSITIVE authored percent = FASTER decay. The consuming nodes
    // author decay as a downside (No Safety +100 "drains twice as fast", No
    // Ground +50 while Grounded) and a suspension as a negative line (Reserve
    // -100 while Aiming), so the floor at 0 is a real state — "does not decay"
    // — not a defensive fiction. Delivered by PushLoopValveOverrides below.
    Stats.ClassResourceDecayMultiplier = FMath::Max(0.0f, Increased(EBreakerNodeStatTarget::ClassResourceDecay));
    Stats.AbilityAreaMultiplier = FMath::Max(0.0f, Increased(EBreakerNodeStatTarget::AbilityArea));
    Stats.AbilityDurationMultiplier = FMath::Max(0.0f, Increased(EBreakerNodeStatTarget::AbilityDuration));
    // The divisor convention (DashCooldownReduction's): 1.20 == 20% shorter.
    // Floored just above zero so no authored row can divide a cooldown by zero.
    Stats.AbilityCooldownReduction = FMath::Max(0.01f, Increased(EBreakerNodeStatTarget::AbilityCooldown));
    // ---- Core-atlas lanes (2026-08-24) -------------------------------------
    // Shapes and consumers on the lane register in BreakerProgressionTypes.h.
    // IncomingDamageReduction is the one Flat lane here: percentage POINTS,
    // summed raw so a downside node may author a negative line; the consumer
    // clamps the composed 1-R application at zero, not this sum.
    Stats.IncomingDamageReductionPercent = Flat(EBreakerNodeStatTarget::IncomingDamageReduction);
    Stats.RecoilRecoveryMultiplier = FMath::Max(0.0f, Increased(EBreakerNodeStatTarget::RecoilRecovery));
    Stats.WeaponSpreadReduction = FMath::Max(0.01f, Increased(EBreakerNodeStatTarget::WeaponSpread));
    Stats.StatusChanceMultiplier = FMath::Max(0.0f, Increased(EBreakerNodeStatTarget::StatusChance));
    Stats.StatusDurationMultiplier = FMath::Max(0.0f, Increased(EBreakerNodeStatTarget::StatusDuration));

    if (OutContribution)
    {
        // Raw buckets, not the composed multipliers: tree percentages have to
        // join gear percentages in ONE additive bucket per stat. The More
        // product is the exception and always was — it is a different bucket,
        // composed multiplicatively through ComposeMore under the O3 cap
        // enforced above.
        OutContribution->Reset();
        OutContribution->AddFlat(EBreakerAggregatedAttribute::MaxHealth, Stats.BonusHealth);
        OutContribution->AddFlat(EBreakerAggregatedAttribute::CriticalChance, Stats.CriticalChanceBonus);
        OutContribution->AddFlat(EBreakerAggregatedAttribute::CriticalMultiplier, Stats.CriticalMultiplierBonus);
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::MoveSpeed, IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::MoveSpeed)]);
        // Slide speed and air control join gear's percentages in the same
        // additive bucket, exactly as move speed already did. Until this
        // existed the movement component multiplied the tree multiplier by the
        // gear multiplier, so +20/+20 read x1.44 against a locked x1.40.
        //
        // There is deliberately NO dash-cooldown line here: EBreakerNodeStatTarget
        // has no dash entry, so no node can author one. The attribute exists and
        // gear bids into it; a tree dash node only needs the enum entry and one
        // line here, and it will be additive from the first day it exists.
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::SlideSpeedMultiplier, IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::SlideSpeed)]);
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::AirControlMultiplier, IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::AirControl)]);
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::DamageOverTimeMultiplier, IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::DamageOverTime)]);

        // ---- O54: the three pools -----------------------------------------
        // Damage is the SHARED pool, so it bids into both delivery lanes from
        // one authored percentage. Weapons compose exactly as they did before
        // the split — shared feeds the weapon lane too — and the thirty rows
        // already authored against this target now reach abilities as well,
        // which is the whole reason this target took the shared meaning rather
        // than the weapon one.
        OutContribution->AddSharedIncreasedDamage(IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::Damage)]);
        // The two narrow lines. Each joins ONE lane's additive bucket, beside
        // the shared bid above and beside gear's affixes in the same bucket.
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier, IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::WeaponDamage)]);
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::AbilityDamageMultiplier, IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::AbilityDamage)]);

        // ---- The six lanes whose attributes ALREADY EXISTED ---------------
        // Every one of these was a stat the aggregator could already carry and
        // gear already bid into, with no way for a NODE to reach it — which is
        // why 53 of 97 authored tree nodes shipped with no effect. One line
        // each. The comment above about there being deliberately no dash line
        // was true when it was written and is now answered: EBreakerNodeStatTarget
        // has a dash entry, so this is that entry's one line.
        //
        // All six join the SAME additive Increased bucket gear bids into, which
        // is the point — a tree line and an affix line on one stat compose
        // additively rather than multiplying, the bug class fixed everywhere
        // else in this codebase.
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::ResourceCostMultiplier, IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::AbilityCost)]);
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::MaxClassResource, IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::MaxClassResource)]);
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::ClassResourceRegen, IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::ClassResourceRegen)]);
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::FireRateMultiplier, IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::FireRate)]);
        // The dash lane. The comment that used to sit here said there was
        // nowhere for a node to bid because EBreakerAggregatedAttribute had no
        // dash entry; it does, and it did then. Gear's Move.DashCooldown affix
        // has been bidding into DashCooldownReduction the whole time, so this
        // line puts the tree in the SAME additive bucket rather than beside it.
        // Stored as the divisor (x1.20 == a 20% shorter cooldown), which is the
        // only shape two layers can share one bucket in.
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::DashCooldownReduction, IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::DashCooldown)]);
        OutContribution->AddFlat(EBreakerAggregatedAttribute::Armor, FlatByTarget[static_cast<int32>(EBreakerNodeStatTarget::Armor)]);
        // The two delivery lanes' More products, already selected out of the one
        // budget above. A shared More has been multiplied into both, which is
        // why there is no third ComposeSharedMoreDamage call here — the
        // duplication happened at selection, once, where the budget is spent.
        if (!FMath::IsNearlyEqual(WeaponMoreProduct, 1.0f))
        {
            OutContribution->ComposeMore(EBreakerAggregatedAttribute::DamageMultiplier, WeaponMoreProduct);
        }
        if (!FMath::IsNearlyEqual(AbilityMoreProduct, 1.0f))
        {
            OutContribution->ComposeMore(EBreakerAggregatedAttribute::AbilityDamageMultiplier, AbilityMoreProduct);
        }
        // A4 (owner ruling 2026-08-16): the DoT More lane rides the
        // DamageOverTimeMultiplier attribute's More product — selected and
        // per-source-clamped above WITH the Damage Mores, one shared budget.
        // ComposeDotSourcePower divides it back out of the composed attribute
        // to keep the Increased half additive, then multiplies it into the
        // tick's More side under the one O34 ceiling. Direct hits never see it.
        //
        // NOTHING IN THE PROGRESSION CONTENT AUTHORS THIS LANE. It was built
        // for Caster.VoidWhisperer.LongDark, which was its only author, and
        // O95 took that node's multiplier away — so this branch is currently
        // unreachable from the tree. It is not dead: the mechanism is exercised
        // by RiorsEdge.Combat.Ceiling.DotAdditiveBucket and stays available to
        // an Anomalous rewrite, which is the layer O3 still permits a More on.
        // But a lane waiting for an author is not a lane in service, and this
        // is the one place a reader can learn that without a grep.
        if (!FMath::IsNearlyEqual(DotMoreProduct, 1.0f))
        {
            OutContribution->ComposeMore(EBreakerAggregatedAttribute::DamageOverTimeMultiplier, DotMoreProduct);
        }
    }
    return Stats;
}

bool UBreakerProgressionComponent::IsNodeMoreAuthoringLegal(const UBreakerProgressionNode* Node, FString* OutReason)
{
    // The rule this validates lives in AggregateStats: a More multiplier is a
    // single-rank purchase by construction, and the fold there refuses to
    // scale one by rank. A node that authors MorePercent at MaxRank > 1 is
    // therefore content that promises ranks it cannot pay (owner ruling
    // 2026-08-16: fail it statically rather than reprice it silently).
    if (!Node) return true;
    if (Node->MaxRank <= 1) return true;
    for (const FBreakerNodeEffect& Effect : Node->Effects)
    {
        if (Effect.StatBucket == EBreakerNodeStatBucket::MorePercent)
        {
            if (OutReason)
            {
                *OutReason = FString::Printf(
                    TEXT("node '%s' authors a MorePercent effect (stat target %d) at MaxRank %d — More multipliers are single-rank only"),
                    *Node->NodeId.ToString(), static_cast<int32>(Effect.StatTarget), Node->MaxRank);
            }
            return false;
        }
    }
    return true;
}

namespace
{
    // O98: the source tag a rider-delivered stat target keys on. One switch so
    // the builder cannot disagree with the vocabulary about which tag selects
    // which slice. MeleeDamage is the only rider-delivered target today; the
    // tag is the native one Cleave and the Tank sweep already stamp on their
    // requests, referenced here rather than restated as a string.
    FGameplayTag BreakerRiderSliceSourceTag(EBreakerNodeStatTarget Target)
    {
        return Target == EBreakerNodeStatTarget::MeleeDamage
            ? BreakerAbilityTags::Damage_Melee.GetTag()
            : FGameplayTag();
    }
}

TArray<FBreakerTargetConditionRider> UBreakerProgressionComponent::BuildTargetConditionRiders(
    const TArray<const UBreakerProgressionNode*>& Nodes, const TArray<FBreakerNodeRank>& Ranks)
{
    // STAGE 6 (Hook-And-Condition-Vocabulary §3.2). The rows the target side
    // resolves: same node walk as AggregateStats, filtered to the effects
    // whose requirement names a Target* condition anywhere in it. The full
    // requirement travels with the row — a rider may mix self and target
    // conditions ("while airborne and against a bleeding enemy"), and
    // ReceiveDamage evaluates it against the attacker's cached SELF state
    // plus the freshly supplied target state.
    TArray<FBreakerTargetConditionRider> Riders;
    for (const FBreakerNodeRank& Rank : Ranks)
    {
        if (Rank.Rank <= 0) continue;
        const UBreakerProgressionNode* const* Found = Nodes.FindByPredicate(
            [&Rank](const UBreakerProgressionNode* Node) { return Node && Node->NodeId == Rank.NodeId; });
        if (!Found) continue;
        const UBreakerProgressionNode* Node = *Found;

        const int32 EffectiveRank = FMath::Min(Rank.Rank, Node->MaxRank);
        for (const FBreakerNodeEffect& Effect : Node->Effects)
        {
            // O98: a rider-delivered slice line rides this table even when its
            // condition is Always — the tag requirement on the row is what
            // defers it to the hit, exactly as a Target* condition defers a
            // conditional line. The aggregator files the same effect into a
            // bucket nothing reads, so this is the line's ONLY payment.
            const bool bTagKeyed = BreakerStatTargetIsRiderDelivered(Effect.StatTarget);
            if (!Effect.RequiresTargetState() && !bTagKeyed) continue;

            // Target-side lines are Increased-bucket damage-pool lines — plus
            // ONE ruled exception (O141): a target-gated MorePercent on a
            // delivered damage pool rides this table as a HIT-TIME More, paid
            // at the combat site by multiplying the request's standing More
            // product under the one O34 ceiling. The old §3.3 objection —
            // that a target-gated More would re-run the strongest-three
            // selection per event — was answered by LEDGER's report: it never
            // enters the sort at all; it spends headroom, exactly as the
            // outgoing window chain already does. Everything ELSE is still
            // dropped LOUDLY, once per node — the dead-lane rule (§2.7):
            //  * Flat and non-damage targets have no lane yet.
            //  * A MorePercent on a tag-keyed slice or DamageOverTime has no
            //    payment shape (a tick snapshots at application).
            const bool bRiderIncreased = Effect.StatBucket == EBreakerNodeStatBucket::IncreasedPercent
                && (BreakerIsDeliveredDamagePool(Effect.StatTarget) || bTagKeyed);
            const bool bRiderMore = Effect.StatBucket == EBreakerNodeStatBucket::MorePercent
                && Effect.RequiresTargetState()
                && BreakerIsDeliveredDamagePool(Effect.StatTarget)
                && !bTagKeyed;
            if (!bRiderIncreased && !bRiderMore)
            {
                static TSet<FName> WarnedOnceTargetRiderNodeIds;
                if (!WarnedOnceTargetRiderNodeIds.Contains(Node->NodeId))
                {
                    WarnedOnceTargetRiderNodeIds.Add(Node->NodeId);
                    UE_LOG(LogTemp, Warning,
                        TEXT("[BreakerProgression] node '%s' authors a target-conditional effect in bucket %d on stat target %d, which no rider shape pays (Increased damage-pool lines, or O141's one hit-time More) — this effect is dropped."),
                        *Node->NodeId.ToString(), static_cast<int32>(Effect.StatBucket), static_cast<int32>(Effect.StatTarget));
                }
                continue;
            }

            FBreakerTargetConditionRider& Rider = Riders.AddDefaulted_GetRef();
            Rider.Condition = Effect.Condition;
            Rider.AlsoRequires = Effect.AlsoRequires;
            Rider.StatTarget = Effect.StatTarget;
            if (bRiderMore)
            {
                // A More never scales with rank (the aggregation-side rule,
                // held here too): the authored percent is the whole value.
                Rider.MorePercent = FMath::Max(0.0f, Effect.ValuePerRank);
            }
            else
            {
                Rider.Percent = Effect.ValuePerRank * static_cast<float>(EffectiveRank);
            }
            Rider.RequiredSourceTag = BreakerRiderSliceSourceTag(Effect.StatTarget);
        }
    }
    return Riders;
}

float UBreakerProgressionComponent::GetSpentPoints() const
{
    // Points actually committed to nodes, in both wallets. GetRefundValue is
    // rank x CostPerRank, which is exactly what a respec hands back — so a
    // 3-point Convergence node is worth three times a 1-point minor here, and
    // the total can never disagree with what the player was charged.
    return static_cast<float>(GetRefundValue(EBreakerPointCurrency::CorePoints)
        + GetRefundValue(EBreakerPointCurrency::DoctrinePoints));
}

float UBreakerProgressionComponent::GetPointSpendDamagePercent() const
{
    return GetSpentPoints() * FMath::Max(0.0f, IncreasedDamagePerSpentPoint);
}

void UBreakerProgressionComponent::RecalculateStats()
{
    TArray<const UBreakerProgressionNode*> Nodes;
    CollectKnownNodes(Nodes, EBreakerPointCurrency::DoctrinePoints);
    CollectKnownNodes(Nodes, EBreakerPointCurrency::CorePoints);

    // EVERY POOL, AND THE OMISSION HERE IS SILENT. This built the aggregation
    // input from two arrays; adding a third pool without adding it here made
    // every doctrine node pay exactly nothing, with no warning and no failed
    // purchase -- the node buys, the rank records, and the effect never reaches
    // the aggregator. The retired pool stays in the list because a v5 save
    // still carries ranks in it until the migration runs.
    TArray<FBreakerNodeRank> Ranks = State.CoreNodeRanks;
    Ranks.Append(State.DoctrineNodeRanks);

    CachedStats = AggregateStats(Nodes, Ranks, &CachedContribution, ActiveConditions);
    // Stage 6: the rider table rides the same recalculation, so purchases,
    // respecs, loads and condition transitions all republish the current rows
    // and nothing new needs invalidating.
    CachedTargetRiders = BuildTargetConditionRiders(Nodes, Ranks);

    // Every committed point pays a small Increased Damage baseline, into the
    // SAME additive bucket as gear and node damage. Under O27 this is a FLOOR,
    // not a power source: at 0.25%/point both a baseline and an optimized build
    // spend their whole budget, so it lands on both equally and differentiates
    // neither. What separates them is which nodes they bought. It stays a
    // property of SPENDING, not of level, which keeps it clear of the cap-50
    // no-post-cap-power ruling.
    const float SpendPercent = GetPointSpendDamagePercent();
    if (SpendPercent > 0.0f)
    {
        // O54: SHARED, not weapon-only. The floor is a property of having spent
        // points, not of how the damage is delivered — paying it into the
        // weapon lane alone would make every point spent a small nerf to an
        // ability build relative to a weapon one, which is the opposite of a
        // floor that "lands on both equally".
        CachedContribution.AddSharedIncreasedDamage(SpendPercent);
        CachedStats.DamageMultiplier += SpendPercent / 100.0f;
        CachedStats.AbilityDamageMultiplier += SpendPercent / 100.0f;
    }

    ApplyStatsToAttributes();
    PushLoopValveOverrides();
}

void UBreakerProgressionComponent::PushLoopValveOverrides()
{
    // The progression -> resource-component bridge for the ClassResourceDecay
    // lane. Decay is a per-loop rule computed inside each class component, not
    // a shared attribute, and PushLoopOverride is the seam those components
    // already expose for "rewrite the loop, keyed and replaceable" — so the
    // tree's composed decay change rides that valve rather than growing a
    // second delivery path (the enum comment on ClassResourceDecay:
    // "plumbing to an existing valve, not a new system").
    //
    // Re-pushing the same key replaces, so a condition transition (Reserve's
    // Aiming line going live, No Ground's Grounded half toggling) or a respec
    // simply re-states the current truth; a neutral multiplier pops the entry
    // entirely so an idle build leaves the override map empty rather than
    // full of 1.0s. No expiry: the override stands until the aggregate moves.
    //
    // Momentum and Grit are the two loops with a decay rule. Mana regenerates
    // and Scrap banks (neither decays), and Charge's out-of-combat settle has
    // no authored bidder yet — recorded on the lane register rather than
    // multiplied in speculatively. Both components are pushed when present;
    // each is inert for an owner of the wrong class anyway, so the extra map
    // entry on a mixed-component test rig is harmless.
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority()) return;
    static const FName BreakerLoopValveKey(TEXT("Progression.ClassResourceDecay"));
    const float DecayMultiplier = CachedStats.ClassResourceDecayMultiplier;
    const bool bNeutral = FMath::IsNearlyEqual(DecayMultiplier, 1.0f);

    if (UBreakerMomentumComponent* Momentum = Owner->FindComponentByClass<UBreakerMomentumComponent>())
    {
        if (bNeutral) Momentum->PopLoopOverride(BreakerLoopValveKey);
        else Momentum->PushLoopOverride(BreakerLoopValveKey, false, 1.0f, 0.0f, DecayMultiplier);
    }
    if (UBreakerGritComponent* Grit = Owner->FindComponentByClass<UBreakerGritComponent>())
    {
        if (bNeutral) Grit->PopLoopOverride(BreakerLoopValveKey);
        else Grit->PushLoopOverride(BreakerLoopValveKey, false, 1.0f, 0.0f, DecayMultiplier);
    }
}

void UBreakerProgressionComponent::ApplyStatsToAttributes()
{
    PublishNodeTagsToAbilitySystem();
    // One submission, no absolute writes. A respec submits an empty
    // contribution, which restores exactly the pre-purchase composition.
    if (!Attributes || !GetOwner() || !GetOwner()->HasAuthority()) return;
    Attributes->ApplyAttributeContribution(EBreakerAttributeContributor::Progression, CachedContribution);
}

void UBreakerProgressionComponent::PublishNodeTagsToAbilitySystem()
{
    // A node's GrantedTags used to live only in FBreakerNodeStats, which meant
    // the one system most of them exist to talk to could not read them:
    // UBreakerAbility_Overdrive resolves its keystone variant from
    // ASC->GetOwnedGameplayTags, and Class-Kits' whole branch-keystone pattern
    // is "the keystone rewrites the ultimate". Publishing the aggregate as
    // loose tags is what makes Bloodrhythm a real rewrite instead of a label.
    //
    // Diffed against the last publication rather than cleared and re-added, so
    // a recalculation that changes nothing touches nothing, and a respec
    // removes exactly the tags this component put there and no others.
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority()) return;
    const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(Owner);
    UAbilitySystemComponent* ASC = AbilityOwner ? AbilityOwner->GetAbilitySystemComponent() : nullptr;
    if (!ASC) return;

    const FGameplayTagContainer& Desired = CachedStats.GrantedTags;
    for (const FGameplayTag& Tag : PublishedNodeTags)
    {
        if (!Desired.HasTagExact(Tag)) ASC->RemoveLooseGameplayTag(Tag);
    }
    for (const FGameplayTag& Tag : Desired)
    {
        if (!PublishedNodeTags.HasTagExact(Tag)) ASC->AddLooseGameplayTag(Tag);
    }
    PublishedNodeTags = Desired;
}

namespace
{
    // Storage for a currency that has none. Shared, never written by any live
    // path, and deliberately not a reference into real state -- see RanksFor.
    TArray<FBreakerNodeRank>& BreakerRetiredRanks()
    {
        static TArray<FBreakerNodeRank> Empty;
        Empty.Reset();
        return Empty;
    }
}

TArray<FBreakerNodeRank>& UBreakerProgressionComponent::RanksFor(EBreakerPointCurrency Currency)
{
    switch (Currency)
    {
    // THE RETIRED CURRENCY RESOLVES TO NOTHING, NOT TO ANOTHER POOL. Falling
    // through to Core would make a stray call operate on the wrong pool's ranks
    // silently, which is a corruption rather than a no-op; returning the retired
    // array would keep alive a read that is correct only because the migration
    // zeroed it. Nothing on a live path passes this value, and this arm is what
    // makes that fact unnecessary to rely on.
    case EBreakerPointCurrency::ClassPoints_Retired: return BreakerRetiredRanks();
    case EBreakerPointCurrency::DoctrinePoints:      return State.DoctrineNodeRanks;
    default:                                         return State.CoreNodeRanks;
    }
}

const TArray<FBreakerNodeRank>& UBreakerProgressionComponent::RanksFor(EBreakerPointCurrency Currency) const
{
    switch (Currency)
    {
    // THE RETIRED CURRENCY RESOLVES TO NOTHING, NOT TO ANOTHER POOL. Falling
    // through to Core would make a stray call operate on the wrong pool's ranks
    // silently, which is a corruption rather than a no-op; returning the retired
    // array would keep alive a read that is correct only because the migration
    // zeroed it. Nothing on a live path passes this value, and this arm is what
    // makes that fact unnecessary to rely on.
    case EBreakerPointCurrency::ClassPoints_Retired: return BreakerRetiredRanks();
    case EBreakerPointCurrency::DoctrinePoints:      return State.DoctrineNodeRanks;
    default:                                         return State.CoreNodeRanks;
    }
}

#undef LOCTEXT_NAMESPACE
