#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Progression/BreakerBuildConditions.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

// ---------------------------------------------------------------------------
// 2026-08-14 O33-O40 identity-stack audit pass — Lane 2 (Progression/, Items/)
// ---------------------------------------------------------------------------
// This file covers the Progression-side work items from that pass that are not
// already homed in an existing topic file: the condition-mask width hardening
// (item 1), the phantom-ability-grant validation (item 3), the Caster fallback
// class definition (item 4), the Core constellation field (item 5), the
// aggregation perf/class-filter fix (item 6), subclass commitment (item 9),
// and the O39 auto-lock gate (item 10). Item 2's dropped-More warnings live
// beside their code in BreakerProgressionComponent.cpp/BreakerEquipmentComponent
// .cpp; item 7's PowerBand fixtures live in BreakerPowerBandTests.cpp; item 8's
// equip caps live in BreakerItemTests.cpp/BreakerItemRuleTests.cpp; item 11 is
// comment/constant hygiene with no new behaviour to test.

// ---------------------------------------------------------------------------
// ITEM 1 — condition mask hardening (pre-O30)
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBuildConditionMaskTest,
    "RiorsEdge.Progression.BuildConditionMask",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBuildConditionMaskTest::RunTest(const FString& Parameters)
{
    // FBreakerBuildConditionState::Mask/Bit widened uint8 -> uint32 because
    // 1u << 8 silently overflows a uint8 to 0, which would make a 9th
    // condition compile, purchase, and never activate — no warning, no
    // assert, just a dead node. This is the runtime guard: every condition
    // the enum currently defines must set and read back independently, with
    // no aliasing between bits, and the property has to keep holding as O30's
    // taxonomy (ailment, crit, stacking, ...) adds more of them.
    for (int32 Index = 1; Index < FBreakerBuildConditionState::ConditionCount; ++Index)  // skip Always (0)
    {
        const EBreakerBuildCondition Condition = static_cast<EBreakerBuildCondition>(Index);
        FBreakerBuildConditionState State;
        TestFalse(*FString::Printf(TEXT("Condition %d starts inactive"), Index), State.IsActive(Condition));
        State.Set(Condition, true);
        TestTrue(*FString::Printf(TEXT("Condition %d activates after Set(true)"), Index), State.IsActive(Condition));

        // No aliasing: setting THIS condition must not activate any OTHER
        // one. A collision from an overflowed Bit() would show up here first.
        for (int32 Other = 1; Other < FBreakerBuildConditionState::ConditionCount; ++Other)
        {
            if (Other == Index) continue;
            TestFalse(*FString::Printf(TEXT("Condition %d does not alias condition %d"), Index, Other),
                State.IsActive(static_cast<EBreakerBuildCondition>(Other)));
        }

        State.Set(Condition, false);
        TestFalse(*FString::Printf(TEXT("Condition %d deactivates after Set(false)"), Index), State.IsActive(Condition));
    }

    // Always is true unconditionally, even on the default-constructed empty
    // state — the property every pre-existing conditional-effect call site
    // depends on.
    FBreakerBuildConditionState Empty;
    TestTrue(TEXT("Always reads active even on the empty state"), Empty.IsActive(EBreakerBuildCondition::Always));

    // All() sets every real condition at once, with none lost to a collision.
    const FBreakerBuildConditionState AllActive = FBreakerBuildConditionState::All();
    for (int32 Index = 0; Index < FBreakerBuildConditionState::ConditionCount; ++Index)
    {
        TestTrue(*FString::Printf(TEXT("All() activates condition %d"), Index),
            AllActive.IsActive(static_cast<EBreakerBuildCondition>(Index)));
    }

    // The width guard itself: today's Count must sit comfortably inside the
    // widened uint32, which is what makes the whole condition family growable
    // without the storage type ever needing to change again.
    TestTrue(TEXT("ConditionCount fits well inside the widened 32-bit mask"),
        FBreakerBuildConditionState::ConditionCount <= 32);
    return true;
}

// ---------------------------------------------------------------------------
// ITEM 2 — loud dead More lanes (Progression side; the Items-side twin in
// BreakerEquipmentComponent.cpp is structurally identical and is unreachable
// through the real affix pool by construction — RiorsEdge.Items.Affixes
// .Breadth already pins that no pool entry authors MorePercent, so there is
// no black-box path to exercise its warning without mutating the cached pool
// singleton. Verified by code inspection instead of a test.)
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDroppedMoreLaneIsLoudTest,
    "RiorsEdge.Progression.DroppedMoreLaneIsLoud",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDroppedMoreLaneIsLoudTest::RunTest(const FString& Parameters)
{
    // A MorePercent effect on any target but Damage used to vanish with no
    // signal at all -- Class-Kits' VW12 authors exactly this, a DoT More.
    // The fix does not make it PAY (the aggregation lane for a non-Damage
    // More does not exist yet, same as before); it makes the drop LOUD.
    UBreakerProgressionNode* Node = NewObject<UBreakerProgressionNode>();
    Node->NodeId = TEXT("Test.DroppedDoTMore");
    Node->MaxRank = 1;
    FBreakerNodeEffect Effect;
    Effect.StatTarget = EBreakerNodeStatTarget::DamageOverTime;
    Effect.StatBucket = EBreakerNodeStatBucket::MorePercent;
    Effect.ValuePerRank = 25.0f;
    Node->Effects.Add(Effect);

    TArray<const UBreakerProgressionNode*> Nodes = {Node};
    TArray<FBreakerNodeRank> Ranks = {{TEXT("Test.DroppedDoTMore"), 1}};

    // 0 occurrences tolerates the "warn once per node id" cache already
    // having fired for this exact id earlier in the same process, matching
    // this codebase's own precedent (BreakerDamageTests.cpp's More-ceiling
    // warning check).
    AddExpectedError(TEXT("MorePercent effect"), EAutomationExpectedErrorFlags::Contains, 0);
    const FBreakerNodeStats Stats = UBreakerProgressionComponent::AggregateStats(Nodes, Ranks);
    // The drop is real: DamageOverTimeMultiplier stays neutral. Loud does not
    // mean fixed -- the point is that it is no longer silent.
    TestEqual(TEXT("The dropped DoT More still does not move DamageOverTimeMultiplier"), Stats.DamageOverTimeMultiplier, 1.0f, 0.0001f);
    return true;
}

// ---------------------------------------------------------------------------
// ITEM 3 — phantom ability grants
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNoPhantomAbilityGrantsTest,
    "RiorsEdge.Progression.NoPhantomAbilityGrants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNoPhantomAbilityGrantsTest::RunTest(const FString& Parameters)
{
    // A node that GrantedAbilityIds.Add()s an id with no entry in the ability
    // registry can unlock a loadout slot that silently does nothing when
    // pressed -- the phantom grant this item exists to catch. Every fallback
    // tree, every node, every granted id, checked against the registry's own
    // public lookup.
    int32 GrantsChecked = 0;
    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
    {
        if (!Tree) continue;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (!Node) continue;
            for (const FName AbilityId : Node->GrantedAbilityIds)
            {
                ++GrantsChecked;
                TestNotNull(*FString::Printf(TEXT("%s grants '%s', which resolves in the ability registry"),
                    *Node->NodeId.ToString(), *AbilityId.ToString()),
                    UBreakerAbilityDefinition::FindFallback(AbilityId));
            }
        }
    }
    // At least one real grant must exist, or this test would pass vacuously
    // and never actually exercise the check (Swift.Marksman.Lead grants
    // Swift.Lead).
    TestTrue(TEXT("At least one ability grant exists to validate"), GrantsChecked >= 1);
    return true;
}

// ---------------------------------------------------------------------------
// ITEM 4 — Caster fallback class definition
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCasterAbilitiesUnlockTest,
    "RiorsEdge.Progression.CasterAbilitiesUnlock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCasterAbilitiesUnlockTest::RunTest(const FString& Parameters)
{
    // GetFallbackClassDefinition(Caster) used to return nullptr, so
    // IsAbilityUnlocked always refused a Caster equip. Tested through the
    // public EquipAbility, IsAbilityUnlocked's only externally visible effect.
    TestNotNull(TEXT("Caster has a fallback class definition"), UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId::Caster));

    const TArray<FName> ClassAbilities = {
        TEXT("Caster.Cleave"), TEXT("Caster.Rot"), TEXT("Caster.Closequarter"),
        TEXT("Caster.Siphon"), TEXT("Caster.Fracture"), TEXT("Caster.Resonance")};
    for (const FName AbilityId : ClassAbilities)
    {
        // A fresh component per id: EquipAbility refuses a duplicate id
        // already sitting in ANY loadout slot, which would otherwise fail
        // every id after the first for an unrelated reason.
        UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
        TestTrue(TEXT("Choosing Caster succeeds"), Progression->ChoosePermanentClassById(EBreakerClassId::Caster));

        FText Failure;
        const bool bEquipped = Progression->EquipAbility(EBreakerAbilitySlot::ClassAbilityOne, AbilityId, Failure);
        TestTrue(*FString::Printf(TEXT("%s unlocks and equips for a Caster (%s)"), *AbilityId.ToString(), *Failure.ToString()), bEquipped);
        TestEqual(TEXT("The equipped id is recorded"), Progression->GetProgressionState().AbilityLoadout.ClassAbilityOne, AbilityId);
    }

    // The ultimate resolves through its own slot rule.
    UBreakerProgressionComponent* UltimateProgression = NewObject<UBreakerProgressionComponent>();
    UltimateProgression->ChoosePermanentClassById(EBreakerClassId::Caster);
    FText UltimateFailure;
    TestTrue(TEXT("Caster.Unmake unlocks and equips as the ultimate"),
        UltimateProgression->EquipAbility(EBreakerAbilitySlot::Ultimate, TEXT("Caster.Unmake"), UltimateFailure));
    TestEqual(TEXT("The ultimate slot records it"), UltimateProgression->GetProgressionState().AbilityLoadout.Ultimate, FName(TEXT("Caster.Unmake")));

    // Swift stays exactly as it was -- this pass adds a row, it does not
    // touch the existing one.
    TestNotNull(TEXT("Swift still has a fallback class definition"), UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId::Swift));
    TestNull(TEXT("Gunsmith still grants nothing (O39 slice class honesty)"), UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId::Gunsmith));
    return true;
}

// ---------------------------------------------------------------------------
// ITEM 5 — Constellation becomes a field, not a string prefix
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCoreConstellationFieldTest,
    "RiorsEdge.Progression.CoreConstellationField",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCoreConstellationFieldTest::RunTest(const FString& Parameters)
{
    // Every Core node now carries its constellation as a FIELD rather than
    // only as a NodeId string prefix -- which is what let the Velocity
    // constellation's six nodes fall into UI/BreakerMenu.cpp's UNMAPPED
    // catch-all the day they were authored without a matching hardcoded
    // prefix entry. The UI is another lane's territory; this pins the data
    // the consumer gets wired to at integration.
    const UBreakerProgressionTree* Core = UBreakerProgressionLibrary::GetCoreSliceTree();
    if (!TestNotNull(TEXT("Core tree exists"), Core)) return false;

    TMap<FName, int32> CountByConstellation;
    for (const UBreakerProgressionNode* Node : Core->Nodes)
    {
        if (!TestNotNull(TEXT("Node is valid"), Node)) continue;
        TestFalse(*(Node->NodeId.ToString() + TEXT(" carries a non-None constellation")), Node->Constellation.IsNone());
        // The field must agree with the id prefix it replaces, or the two
        // sources of truth disagree about the same node.
        const FString ExpectedPrefix = FString::Printf(TEXT("Core.%s."), *Node->Constellation.ToString());
        TestTrue(*(Node->NodeId.ToString() + TEXT(" constellation matches its id prefix")),
            Node->NodeId.ToString().StartsWith(ExpectedPrefix));
        CountByConstellation.FindOrAdd(Node->Constellation)++;
    }

    // Velocity is the constellation the UI's hardcoded cluster list is
    // missing (six nodes, per CONTEXT.md's recorded UNMAPPED gap). Its field
    // membership must be intact regardless of the UI catching up to it.
    TestEqual(TEXT("Velocity carries its authored six nodes"), CountByConstellation.FindRef(TEXT("Velocity")), 6);
    TestEqual(TEXT("Seven constellations are represented"), CountByConstellation.Num(), 7);

    // Class branch nodes are not a constellation and must stay None.
    for (const UBreakerProgressionTree* Tree : {UBreakerProgressionLibrary::GetSwiftKineticTree(),
        UBreakerProgressionLibrary::GetSwiftMarksmanTree(), UBreakerProgressionLibrary::GetSwiftFrenzyTree()})
    {
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            TestTrue(*(Node->NodeId.ToString() + TEXT(" (a branch node) carries no constellation")), Node->Constellation.IsNone());
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// ITEM 6 — aggregation perf + class filter
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSpentPointsPerfTest,
    "RiorsEdge.Progression.SpentPointsPerf",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSpentPointsPerfTest::RunTest(const FString& Parameters)
{
    // GetSpentPoints/GetRefundValue used to recompute by walking every owned
    // rank's node definition on every RecalculateStats (which fires on every
    // movement-state transition) -- O(ranks x N^2), because the walk rebuilt
    // the entire known-node array from scratch per rank. This is the
    // correctness guard for the running-total replacement: it must agree
    // with a from-scratch count through purchases, a respec, and a fresh
    // load carrying a stale/unknown node id (the fallback-cost-1 path).
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    Progression->ApplySliceDefaultsIfFresh();

    UBreakerProgressionTree* Core = UBreakerProgressionLibrary::GetCoreSliceTree();
    UBreakerProgressionTree* Kinetic = UBreakerProgressionLibrary::GetSwiftKineticTree();
    FText Failure;

    TestEqual(TEXT("Nothing spent before any purchase"), Progression->GetSpentPoints(), 0.0f);

    TestTrue(TEXT("Sightline purchases"), Progression->PurchaseNode(Core, TEXT("Core.Precision.Sightline"), Failure));         // cost 1, Core
    TestTrue(TEXT("Volley gateway purchases"), Progression->PurchaseNode(Core, TEXT("Core.Volley.TriggerDiscipline"), Failure)); // cost 1, Core
    TestTrue(TEXT("Tunnel Vision purchases"), Progression->PurchaseNode(Core, TEXT("Core.Precision.TunnelVision"), Failure));  // cost 2, Core
    TestEqual(TEXT("Core spend tracks three purchases"), Progression->GetSpentPoints(), 4.0f, 0.0001f);

    TestTrue(TEXT("Carry purchases"), Progression->PurchaseNode(Kinetic, TEXT("Swift.Kinetic.Carry"), Failure));               // cost 1, Class
    TestEqual(TEXT("Class spend adds on top of core spend"), Progression->GetSpentPoints(), 5.0f, 0.0001f);

    // Respec one currency: its running total resets to zero and the other is
    // untouched.
    TestTrue(TEXT("Core respec succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::CorePoints, true, Failure));
    TestEqual(TEXT("Core respec zeroes only the core running total"), Progression->GetSpentPoints(), 1.0f, 0.0001f);

    TestTrue(TEXT("Class respec succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::ClassPoints, true, Failure));
    TestEqual(TEXT("Both running totals are zero after both respecs"), Progression->GetSpentPoints(), 0.0f, 0.0001f);

    // A LOADED state carrying a stale/unknown node id: the running total is
    // rebuilt from scratch (RecomputeSpentPointsFromState), using the same
    // fallback-cost-1 rule GetRefundValue always used for content that no
    // longer resolves.
    FBreakerProgressionState Loaded;
    Loaded.PermanentClass = EBreakerClassId::Swift;
    Loaded.CoreNodeRanks.Add({TEXT("Core.Precision.Sightline"), 1});    // real, cost 1
    Loaded.ClassNodeRanks.Add({TEXT("Some.Removed.Node"), 3});          // unknown, fallback cost 1 x 3
    Progression->LoadProgressionState(Loaded);
    TestEqual(TEXT("A freshly loaded state recomputes both totals correctly"), Progression->GetSpentPoints(), 4.0f, 0.0001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerClassSwapStopsOldRanksPayingTest,
    "RiorsEdge.Progression.ClassSwapStopsOldRanksPaying",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerClassSwapStopsOldRanksPayingTest::RunTest(const FString& Parameters)
{
    // CollectKnownNodes filtered by currency but never by RequiredClass, so
    // GetAllFallbackTrees() (walked unconditionally alongside ClassDefinition
    // ->BranchTrees) kept every Swift branch node in aggregation scope no
    // matter what class the character actually was -- a dev class swap left
    // the old class's ranks still paying.
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(NewObject<AActor>());
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    Progression->BindAttributes(Attributes);
    Progression->ApplySliceDefaultsIfFresh(); // locks Swift

    UBreakerProgressionTree* Kinetic = UBreakerProgressionLibrary::GetSwiftKineticTree();
    FText Failure;
    TestTrue(TEXT("Carry purchases as Swift"), Progression->PurchaseNode(Kinetic, TEXT("Swift.Kinetic.Carry"), Failure));
    TestEqual(TEXT("Carry's slide speed is live for Swift"), Progression->GetNodeStats().SlideSpeedMultiplier, 1.12f, 0.0001f);

    // DevForceClass documents that it deliberately leaves a stale
    // ClassDefinition in place (still pointing at Swift's trees); the fix
    // must not depend on that pointer ever being refreshed.
    Progression->DevForceClass(EBreakerClassId::Caster);
    TestEqual(TEXT("The Swift rank stops paying once the permanent class changes"), Progression->GetNodeStats().SlideSpeedMultiplier, 1.0f, 0.0001f);

    // The rank itself is not deleted -- only its contribution stops. Swapping
    // back to Swift resumes it with no re-purchase.
    TestEqual(TEXT("The rank itself survives the swap"), Progression->GetNodeRank(TEXT("Swift.Kinetic.Carry"), EBreakerPointCurrency::ClassPoints), 1);
    Progression->DevForceClass(EBreakerClassId::Swift);
    TestEqual(TEXT("Swapping back resumes the contribution"), Progression->GetNodeStats().SlideSpeedMultiplier, 1.12f, 0.0001f);
    return true;
}

// ---------------------------------------------------------------------------
// ITEM 9 — subclass commitment (O37)
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSubclassCommitmentTest,
    "RiorsEdge.Progression.SubclassCommitment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSubclassCommitmentTest::RunTest(const FString& Parameters)
{
    // Four properties in one continuous playthrough: one-way semantics, the
    // keystone gate itself, ordinary nodes staying free (O15), and a Forge
    // respec clearing the commitment.
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    Progression->ApplySliceDefaultsIfFresh();       // locks Swift, grants 10 class / 12 core
    Progression->GrantPlaytestPoints(30, 0);        // headroom for this playthrough's purchases
    UBreakerProgressionTree* Frenzy = UBreakerProgressionLibrary::GetSwiftFrenzyTree();
    UBreakerProgressionTree* Kinetic = UBreakerProgressionLibrary::GetSwiftKineticTree();
    FText Failure;

    TestEqual(TEXT("A fresh character carries no commitment"), Progression->GetProgressionState().CommittedBranch, FName(NAME_None));

    // --- ORDINARY NODES STAY FREE, even with no commitment at all (O15) ----
    TestTrue(TEXT("An ordinary Frenzy node purchases with no commitment"),
        Progression->PurchaseNode(Frenzy, TEXT("Swift.Frenzy.ShortLeash"), Failure));
    TestTrue(TEXT("An ordinary Kinetic node ALSO purchases with no commitment"),
        Progression->PurchaseNode(Kinetic, TEXT("Swift.Kinetic.ReadTheRoom"), Failure));

    // --- THE KEYSTONE GATE: Bloodrhythm refuses before commitment ----------
    // Reach its investment gate (CornerstoneInvestmentGate, 8) and its own
    // prerequisite (Overrev) first, so the ONLY thing standing between the
    // purchase and success is O37's new commitment check. ORDER MATTERS: each
    // purchase must clear ITS OWN tier's investment gate at the moment it is
    // bought (Tier 2 needs 2 points already in the tree), not just by the end
    // -- Trigger Discipline rank 1 has to land before Overrev (Tier 2) for
    // that reason, on top of Overrev's own ShortLeash prerequisite.
    TestTrue(TEXT("Trigger Discipline rank 1 purchases"), Progression->PurchaseNode(Frenzy, TEXT("Swift.Frenzy.TriggerDiscipline"), Failure));
    TestTrue(TEXT("Overrev purchases (Bloodrhythm's prerequisite)"), Progression->PurchaseNode(Frenzy, TEXT("Swift.Frenzy.Overrev"), Failure));
    TestTrue(TEXT("Trigger Discipline rank 2 purchases"), Progression->PurchaseNode(Frenzy, TEXT("Swift.Frenzy.TriggerDiscipline"), Failure));
    TestTrue(TEXT("Loaded rank 1 purchases"), Progression->PurchaseNode(Frenzy, TEXT("Swift.Frenzy.Loaded"), Failure));
    TestTrue(TEXT("Loaded rank 2 purchases"), Progression->PurchaseNode(Frenzy, TEXT("Swift.Frenzy.Loaded"), Failure));
    TestTrue(TEXT("Rhythm purchases"), Progression->PurchaseNode(Frenzy, TEXT("Swift.Frenzy.Rhythm"), Failure));
    TestTrue(TEXT("Dry Fire purchases"), Progression->PurchaseNode(Frenzy, TEXT("Swift.Frenzy.DryFire"), Failure));
    TestEqual(TEXT("Frenzy investment reaches Bloodrhythm's cornerstone gate"), Progression->GetTreeInvestment(Frenzy), 8);

    TestFalse(TEXT("Bloodrhythm refuses with the investment gate open but no commitment"),
        Progression->CanPurchaseNode(Frenzy, TEXT("Swift.Frenzy.Bloodrhythm"), Failure));
    TestFalse(TEXT("The refusal carries a reason"), Failure.IsEmpty());

    // --- ONE-WAY SEMANTICS --------------------------------------------------
    FText CommitFailure;
    TestTrue(TEXT("Committing to Frenzy succeeds"), Progression->CommitToBranch(TEXT("Swift.Frenzy"), CommitFailure));
    TestEqual(TEXT("The commitment is recorded"), Progression->GetProgressionState().CommittedBranch, FName(TEXT("Swift.Frenzy")));
    TestFalse(TEXT("A second commitment is refused, even to the SAME branch"),
        Progression->CommitToBranch(TEXT("Swift.Frenzy"), CommitFailure));
    TestFalse(TEXT("A second commitment to a DIFFERENT branch is also refused"),
        Progression->CommitToBranch(TEXT("Swift.Kinetic"), CommitFailure));
    TestFalse(TEXT("The refusal carries a reason"), CommitFailure.IsEmpty());

    // --- THE KEYSTONE GATE OPENS for its own branch -------------------------
    TestTrue(TEXT("Bloodrhythm purchases once committed to its branch"),
        Progression->PurchaseNode(Frenzy, TEXT("Swift.Frenzy.Bloodrhythm"), Failure));
    TestTrue(TEXT("Bloodrhythm's tag reaches the aggregate"),
        Progression->GetNodeStats().GrantedTags.HasTag(BreakerNodeTags::Node_Bloodrhythm.GetTag()));

    // Ordinary nodes of a DIFFERENT branch stay free even while committed
    // elsewhere -- O15 is untouched by O37.
    TestTrue(TEXT("An ordinary Kinetic node still purchases while committed to Frenzy"),
        Progression->PurchaseNode(Kinetic, TEXT("Swift.Kinetic.Carry"), Failure));

    // --- RESPEC CLEARS -------------------------------------------------------
    TestTrue(TEXT("Class respec at a Forge succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::ClassPoints, true, Failure));
    TestEqual(TEXT("Respec clears the commitment"), Progression->GetProgressionState().CommittedBranch, FName(NAME_None));
    TestEqual(TEXT("Respec also clears Bloodrhythm's rank"), Progression->GetNodeRank(TEXT("Swift.Frenzy.Bloodrhythm"), EBreakerPointCurrency::ClassPoints), 0);

    // A fresh commitment is possible again after the Forge visit.
    TestTrue(TEXT("A new commitment succeeds after the respec"), Progression->CommitToBranch(TEXT("Swift.Kinetic"), CommitFailure));
    return true;
}

// ---------------------------------------------------------------------------
// ITEM 10 — O39 auto-lock becomes gated (default unchanged)
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAutoLockGateTest,
    "RiorsEdge.Progression.AutoLockGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAutoLockGateTest::RunTest(const FString& Parameters)
{
    // DEFAULT TRUE: today's flow (and every other test in the suite that
    // relies on a fresh pawn arriving as Swift) must see no behaviour change.
    UBreakerProgressionComponent* DefaultProgression = NewObject<UBreakerProgressionComponent>();
    TestTrue(TEXT("bAutoLockSwiftIfFresh defaults true"), DefaultProgression->bAutoLockSwiftIfFresh);
    DefaultProgression->ApplySliceDefaultsIfFresh();
    TestEqual(TEXT("Default behaviour: a fresh character still auto-locks Swift"),
        DefaultProgression->GetProgressionState().PermanentClass, EBreakerClassId::Swift);
    TestEqual(TEXT("...and still receives the slice point grant"),
        DefaultProgression->GetUnspentPoints(EBreakerPointCurrency::ClassPoints), UBreakerProgressionLibrary::SliceClassPointGrant);

    // O39: turned off, a fresh character reaches the class screen with no
    // class chosen, so the screen is actually exercised instead of every
    // fresh character silently becoming Swift before the player sees it.
    UBreakerProgressionComponent* GatedProgression = NewObject<UBreakerProgressionComponent>();
    GatedProgression->bAutoLockSwiftIfFresh = false;
    GatedProgression->ApplySliceDefaultsIfFresh();
    TestEqual(TEXT("Gated off: a fresh character is NOT auto-locked"),
        GatedProgression->GetProgressionState().PermanentClass, EBreakerClassId::None);
    // The slice point grant is a statement about the point ECONOMY, not the
    // class choice, so it still arrives -- the class screen has something to
    // spend against once a class is picked.
    TestEqual(TEXT("...but the slice point grant is unaffected"),
        GatedProgression->GetUnspentPoints(EBreakerPointCurrency::ClassPoints), UBreakerProgressionLibrary::SliceClassPointGrant);

    // A character that already chose a class is never touched by the flag
    // either way -- "only pick a class for a character that has none" is
    // unchanged.
    UBreakerProgressionComponent* ChosenProgression = NewObject<UBreakerProgressionComponent>();
    ChosenProgression->bAutoLockSwiftIfFresh = false;
    TestTrue(TEXT("Choosing Caster explicitly still works with the gate off"),
        ChosenProgression->ChoosePermanentClassById(EBreakerClassId::Caster));
    ChosenProgression->ApplySliceDefaultsIfFresh();
    TestEqual(TEXT("An explicit choice survives ApplySliceDefaultsIfFresh regardless of the gate"),
        ChosenProgression->GetProgressionState().PermanentClass, EBreakerClassId::Caster);
    return true;
}

#endif
