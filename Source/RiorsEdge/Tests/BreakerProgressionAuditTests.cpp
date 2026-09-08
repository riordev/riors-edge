#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Progression/BreakerBuildConditions.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerCoreWheelMath.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestJournal.h"

// ---------------------------------------------------------------------------
// 2026-08-14 O33-O40 identity-stack audit pass â€” Lane 2 (Progression/, Items/)
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
// ITEM 1 â€” condition mask hardening (pre-O30)
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBuildConditionMaskTest,
    "RiorsEdge.Progression.BuildConditionMask",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBuildConditionMaskTest::RunTest(const FString& Parameters)
{
    // FBreakerBuildConditionState::Mask/Bit widened uint8 -> uint32 because
    // 1u << 8 silently overflows a uint8 to 0, which would make a 9th
    // condition compile, purchase, and never activate â€” no warning, no
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
    // state â€” the property every pre-existing conditional-effect call site
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
// ITEM 2 â€” loud dead More lanes (Progression side; the Items-side twin in
// BreakerEquipmentComponent.cpp is structurally identical and is unreachable
// through the real affix pool by construction â€” RiorsEdge.Items.Affixes
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
    // A MorePercent effect on a target with no More lane used to vanish with
    // no signal at all. The historical example here was VW12's DoT More --
    // that one now PAYS under A4 (owner ruling 2026-08-16: Damage and
    // DamageOverTime both compose a More product), so the loud-drop coverage
    // moves to a target that still has no More lane. MoveSpeed is a fair
    // stand-in: a real lane for Increased, no More lane, and none planned.
    UBreakerProgressionNode* Node = NewObject<UBreakerProgressionNode>();
    Node->NodeId = TEXT("Test.DroppedMoveSpeedMore");
    Node->MaxRank = 1;
    FBreakerNodeEffect Effect;
    Effect.StatTarget = EBreakerNodeStatTarget::MoveSpeed;
    Effect.StatBucket = EBreakerNodeStatBucket::MorePercent;
    Effect.ValuePerRank = 25.0f;
    Node->Effects.Add(Effect);

    TArray<const UBreakerProgressionNode*> Nodes = {Node};
    TArray<FBreakerNodeRank> Ranks = {{TEXT("Test.DroppedMoveSpeedMore"), 1}};

    // 0 occurrences tolerates the "warn once per node id" cache already
    // having fired for this exact id earlier in the same process, matching
    // this codebase's own precedent (BreakerDamageTests.cpp's More-ceiling
    // warning check).
    AddExpectedError(TEXT("MorePercent effect"), EAutomationExpectedErrorFlags::Contains, 0);
    const FBreakerNodeStats Stats = UBreakerProgressionComponent::AggregateStats(Nodes, Ranks);
    // The drop is real: MoveSpeedMultiplier stays neutral and the source is
    // not counted into the More budget. Loud does not mean fixed -- the point
    // is that it is no longer silent.
    TestEqual(TEXT("The dropped MoveSpeed More still does not move MoveSpeedMultiplier"), Stats.MoveSpeedMultiplier, 1.0f, 0.0001f);
    TestEqual(TEXT("A laneless More is not a source in the O34 budget"), Stats.DamageMoreSourceCount, 0);

    // And the A4 counterpart, asserted where the old expectation lived: a
    // DamageOverTime More now COMPOSES instead of dropping -- it counts as a
    // held source and rides the DoT lane's More product in the contribution.
    UBreakerProgressionNode* DotNode = NewObject<UBreakerProgressionNode>();
    DotNode->NodeId = TEXT("Test.ComposedDoTMore");
    DotNode->MaxRank = 1;
    FBreakerNodeEffect DotEffect;
    DotEffect.StatTarget = EBreakerNodeStatTarget::DamageOverTime;
    DotEffect.StatBucket = EBreakerNodeStatBucket::MorePercent;
    DotEffect.ValuePerRank = 25.0f;
    DotNode->Effects.Add(DotEffect);
    TArray<const UBreakerProgressionNode*> DotNodes = {DotNode};
    TArray<FBreakerNodeRank> DotRanks = {{TEXT("Test.ComposedDoTMore"), 1}};
    FBreakerAttributeContribution Contribution;
    const FBreakerNodeStats DotStats = UBreakerProgressionComponent::AggregateStats(DotNodes, DotRanks, &Contribution);
    TestEqual(TEXT("A4: a DoT More counts as a held More source"), DotStats.DamageMoreSourceCount, 1);
    TestEqual(TEXT("A4: the DoT More rides the DoT lane's contribution, not the Damage lane's"),
        Contribution.GetMore(EBreakerAggregatedAttribute::DamageOverTimeMultiplier), 1.25f, 0.0001f);
    TestEqual(TEXT("A4: the direct-hit More contribution stays neutral"),
        Contribution.GetMore(EBreakerAggregatedAttribute::DamageMultiplier), 1.0f, 0.0001f);
    return true;
}

// ---------------------------------------------------------------------------
// ITEM 3 â€” phantom ability grants
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
    // THE WRITER POPULATION IS ZERO BY RULING (O140): the last grant â€”
    // Swift.Marksman.Lead's â€” retired when ruling 1 made Lead a token
    // unlockable and the grant became a free route around the quartermaster.
    // The check above is therefore vacuous ON PURPOSE, stated rather than
    // discovered: it stands armed for the next writer, and this count pin is
    // what makes a new grant announce itself here instead of arriving
    // unaudited. Whether the readers-with-no-writers path itself survives is
    // the owner's open question in DECISIONS.
    TestEqual(TEXT("No node ability-grant exists (O140) â€” a new writer moves this pin deliberately"), GrantsChecked, 0);
    return true;
}

// ---------------------------------------------------------------------------
// ITEM 4 â€” Caster fallback class definition
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

    // D11 (2026-08-16): ChoosePermanentClassById now SEEDS the starter loadout
    // exactly as ChoosePermanentClass always did, so Cleave/Rot/Unmake arrive
    // already equipped and the duplicate rule (an id may sit in only one slot)
    // refuses re-equipping them elsewhere. The equip loop therefore covers the
    // four NON-seeded ids, and the two starters plus the ultimate are asserted
    // as seeded â€” which is the stronger form of what this test always meant:
    // every one of the seven is reachable on a fresh Caster.
    {
        UBreakerProgressionComponent* Seeded = NewObject<UBreakerProgressionComponent>();
        TestTrue(TEXT("Choosing Caster succeeds"), Seeded->ChoosePermanentClassById(EBreakerClassId::Caster));
        TestEqual(TEXT("Cleave is seeded into slot one (D11)"), Seeded->GetProgressionState().AbilityLoadout.ClassAbilityOne, FName(TEXT("Caster.Cleave")));
        TestEqual(TEXT("Rot is seeded into slot two (D11)"), Seeded->GetProgressionState().AbilityLoadout.ClassAbilityTwo, FName(TEXT("Caster.Rot")));
        TestEqual(TEXT("Unmake is seeded as the ultimate (D11)"), Seeded->GetProgressionState().AbilityLoadout.Ultimate, FName(TEXT("Caster.Unmake")));
    }

    // O100 CHANGED WHAT "REACHABLE" MEANS, AND THIS TEST FOLLOWED IT RATHER
    // THAN BEING WIDENED. The four non-starters are no longer free at level
    // one â€” they are bought with a token at the quartermaster. So the assertion
    // is now the honest one: each is reachable BY PLAYING, at the level the
    // shipped schedule pays its token, and equips once bought.
    //
    // Nothing here grants more than the game grants: the level comes from the
    // shipped XP curve and the token from the shipped entitlement. Handing the
    // component a token directly would prove the equip path and say nothing
    // about whether a player can ever get one.
    const TArray<FName> ClassAbilities = {
        TEXT("Caster.Closequarter"), TEXT("Caster.Siphon"),
        TEXT("Caster.Fracture"), TEXT("Caster.Resonance")};
    for (const FName AbilityId : ClassAbilities)
    {
        // A fresh component per id: EquipAbility refuses a duplicate id
        // already sitting in ANY loadout slot, which would otherwise fail
        // every id after the first for an unrelated reason.
        UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
        TestTrue(TEXT("Choosing Caster succeeds"), Progression->ChoosePermanentClassById(EBreakerClassId::Caster));

        FText EarlyFailure;
        TestFalse(*FString::Printf(TEXT("%s is locked before it is bought"), *AbilityId.ToString()),
            Progression->EquipAbility(EBreakerAbilitySlot::ClassAbilityOne, AbilityId, EarlyFailure));

        // Level to the last entry of the TOKEN schedule, read from the token
        // schedule. It used to read ClassPointCapLevel, which was a different
        // system's constant that happened to share the number 30 -- so when
        // O111 retired that constant to zero this test silently levelled to
        // zero and every unlock failed for want of tokens. A test that borrows
        // another system's number is asserting a coincidence.
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(
            UBreakerProgressionLibrary::AbilityCompletionLevel,
            Progression->ExperienceCurve));

        FText Failure;
        TestTrue(*FString::Printf(TEXT("%s can be bought at the shipped entitlement (%s)"), *AbilityId.ToString(), *Failure.ToString()),
            Progression->SpendAbilityToken(AbilityId, Failure));
        const bool bEquipped = Progression->EquipAbility(EBreakerAbilitySlot::ClassAbilityOne, AbilityId, Failure);
        TestTrue(*FString::Printf(TEXT("%s equips once unlocked (%s)"), *AbilityId.ToString(), *Failure.ToString()), bEquipped);
        TestEqual(TEXT("The equipped id is recorded"), Progression->GetProgressionState().AbilityLoadout.ClassAbilityOne, AbilityId);
    }

    // Swift stays exactly as it was -- this pass adds a row, it does not
    // touch the existing one.
    TestNotNull(TEXT("Swift still has a fallback class definition"), UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId::Swift));
    // FLIPPED 2026-08-16 (owner authorization: "feel free to do all 5
    // classes"): Gunsmith's kit executes now, so O39's gate admits it and the
    // old "still grants nothing" assertion fired exactly as intended.
    TestNotNull(TEXT("Gunsmith has a fallback class definition (kit implemented 2026-08-16)"), UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId::Gunsmith));
    return true;
}

// ---------------------------------------------------------------------------
// ITEM 5 â€” Constellation becomes a field, not a string prefix
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

    const TArray<FName> ExpectedRing = {
        TEXT("Precision"), TEXT("Vector"), TEXT("Ballistics"), TEXT("Loadout"),
        TEXT("Aegis"), TEXT("Bulwark"), TEXT("Constitution"), TEXT("Ward"), TEXT("Recovery"),
        TEXT("Arc"), TEXT("Tempo"), TEXT("Reservoir"), TEXT("Duration"),
        TEXT("Affliction"), TEXT("Entropy"), TEXT("Reaction"), TEXT("Rift"), TEXT("Void"),
        TEXT("Velocity"), TEXT("Kinesis"), TEXT("Control"), TEXT("Threat")};
    const TSet<FName> Majors = {TEXT("Precision"), TEXT("Vector"), TEXT("Ballistics"), TEXT("Aegis"), TEXT("Bulwark"),
        TEXT("Arc"), TEXT("Tempo"), TEXT("Affliction"), TEXT("Entropy"), TEXT("Reaction"), TEXT("Velocity")};
    TestTrue(TEXT("Every wedge follows the accepted clockwise order"), Core->CoreWedgeOrder == ExpectedRing);
    TestEqual(TEXT("Exactly twenty-two named constellations"), CountByConstellation.Num(), 22);
    TestEqual(TEXT("Every wedge has explicit sector metadata"), Core->CoreWedgeSectors.Num(), 22);
    for (FName Wedge : ExpectedRing)
        TestEqual(*(Wedge.ToString() + TEXT(" has its exact major/minor node count")), CountByConstellation.FindRef(Wedge), Majors.Contains(Wedge) ? 11 : 6);
    const TArray<FName> Sectors = {TEXT("Weapon"), TEXT("Defence"), TEXT("Ability"), TEXT("Status"), TEXT("Movement"), TEXT("Utility")};
    const TArray<int32> ExpectedWedges = {4, 5, 4, 5, 2, 2};
    TMap<FName, int32> WedgesPerSector;
    TArray<FName> OrderedSectors;
    for (FName Wedge : Core->CoreWedgeOrder)
    {
        const FName Sector = Core->CoreWedgeSectors.FindRef(Wedge);
        TestFalse(*(Wedge.ToString() + TEXT(" maps to a sector")), Sector.IsNone());
        TestTrue(*(Wedge.ToString() + TEXT(" maps to an accepted sector")), Sectors.Contains(Sector));
        ++WedgesPerSector.FindOrAdd(Sector);
        if (OrderedSectors.IsEmpty() || OrderedSectors.Last() != Sector) OrderedSectors.Add(Sector);
    }
    TestTrue(TEXT("Six sector blocks appear once in owner order"), OrderedSectors == Sectors);
    for (int32 Index = 0; Index < Sectors.Num(); ++Index)
        TestEqual(*(Sectors[Index].ToString() + TEXT(" has its exact wedge count")), WedgesPerSector.FindRef(Sectors[Index]), ExpectedWedges[Index]);
    TestTrue(TEXT("An unknown constellation has no authored sector"), Core->CoreWedgeSectors.FindRef(TEXT("Travel")).IsNone());

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
// ITEM 6 â€” aggregation perf + class filter
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

    // A ranked lane, its notable, a sibling minor and the available link
    // exercise different prices through the actual replacement graph.
    TestTrue(TEXT("The entry purchases"), Progression->PurchaseNode(Core, TEXT("Core.Precision.Sightline"), Failure));
    TestTrue(TEXT("Angle purchases"), Progression->PurchaseNode(Core, TEXT("Core.Precision.Angle"), Failure));
    TestTrue(TEXT("Called Shot purchases after rank one"), Progression->PurchaseNode(Core, TEXT("Core.Precision.CalledShot"), Failure));
    TestTrue(TEXT("Cadence purchases"), Progression->PurchaseNode(Core, TEXT("Core.Precision.Cadence"), Failure));
    TestTrue(TEXT("One completed lane opens Steady link"), Progression->PurchaseNode(Core, TEXT("Core.Precision.Steady"), Failure));
    TestEqual(TEXT("Core spend tracks five differently priced purchases"), Progression->GetSpentPoints(), 6.0f, 0.0001f);

    // Completed-campaign state is the entitlement fixture; this is not a
    // campaign playthrough. Commitment itself pays nothing.
    if (!TestTrue(TEXT("Commit to the real Kinetic branch"), Progression->CommitToBranch(TEXT("Doctrine.Swift.Kinetic"), Failure))) return false;
    auto* Journal = NewObject<UBreakerQuestJournal>();
    FBreakerQuestFlagSet CompletedCampaign;
    for (const FBreakerMissionDefinition& Mission : UBreakerMissionLibrary::GetMissions())
        for (const FBreakerMissionBeat& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) CompletedCampaign.Add(Flag);
    CompletedCampaign.Add(TEXT("Quest.Finale.Seal"));
    Journal->RestoreFrom(CompletedCampaign);
    Progression->SettleDoctrineEntitlement(Journal->GetState());
    TestEqual(TEXT("Completed benchmarks settle exactly eight"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), UBreakerProgressionLibrary::DoctrinePointGrant);
    TestTrue(TEXT("Carry purchases"), Progression->PurchaseNode(Kinetic, TEXT("Swift.Kinetic.Carry"), Failure));
    TestEqual(TEXT("Doctrine spend adds on top of core spend"), Progression->GetSpentPoints(), 7.0f, 0.0001f);

    // Respec one currency: its running total resets to zero and the other is
    // untouched. The Core respec is free at this fixture's level (O213).
    TestTrue(TEXT("Core respec succeeds"), Progression->RespecCore(Failure));
    // Still two pools, so a Core respec still leaves the doctrine total alone.
    TestEqual(TEXT("Core respec zeroes only the core running total"), Progression->GetSpentPoints(), 1.0f, 0.0001f);

    TestTrue(TEXT("Doctrine respec succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, Failure));
    TestEqual(TEXT("Both running totals are zero after both respecs"), Progression->GetSpentPoints(), 0.0f, 0.0001f);

    // A LOADED state carrying a stale/unknown node id: RULED in the
    // blocked-questions pass (Part One-U item 20) â€” the row is DROPPED AND
    // CREDITED at the fallback cost the recompute would have charged, so a
    // removed node can never silently tax the save that bought it. This
    // assertion used to RECORD the tax (spent 4.0, the unknown row charged
    // 1 x 3 and granted nothing, forever); it now pins the repair, ahead of
    // the first real deletion.
    FBreakerProgressionState Loaded;
    Loaded.PermanentClass = EBreakerClassId::Swift;
    Loaded.CoreNodeRanks.Add({TEXT("Core.Precision.Sightline"), 1});    // real, cost 1
    Loaded.DoctrineNodeRanks.Add({TEXT("Some.Removed.Node"), 3});          // unknown: dropped, +3 credited
    // THE SHIPPED CASE: a travel bead a save bought before O213 deleted the
    // Core.Travel.* ids. Dropped and credited exactly like the hand-made id
    // above â€” one Core point back, no rank left â€” so the first real deletion
    // this repair was written ahead of is what it is measured against.
    Loaded.CoreNodeRanks.Add({TEXT("Core.Travel.Ring0P1Weapon"), 1});   // deleted (O213): dropped, +1 Core credited
    const int32 DoctrineWalletBefore = Loaded.UnspentDoctrinePoints;
    const int32 CoreWalletBefore = Loaded.UnspentCorePoints;
    Progression->LoadProgressionState(Loaded);
    TestEqual(TEXT("the resolving row alone is charged"), Progression->GetSpentPoints(), 1.0f, 0.0001f);
    TestEqual(TEXT("the unknown row is gone from the loaded state"),
        Progression->GetNodeRank(TEXT("Some.Removed.Node"), EBreakerPointCurrency::DoctrinePoints), 0);
    TestEqual(TEXT("its ranks came back as doctrine points, at the fallback cost"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), DoctrineWalletBefore + 3);
    TestEqual(TEXT("the deleted travel bead leaves no rank"),
        Progression->GetNodeRank(TEXT("Core.Travel.Ring0P1Weapon"), EBreakerPointCurrency::CorePoints), 0);
    // The loaded state is not fresh (a Core rank resolves), so no slice lump
    // and no level entitlement land on top: the wallet moves by the credit alone.
    TestEqual(TEXT("the deleted travel bead's point comes back as one Core point"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), CoreWalletBefore + 1);
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
    // O111: doctrine nodes spend the doctrine wallet, at the shipped budget.
    Progression->GrantPlaytestPoints(UBreakerProgressionLibrary::DoctrinePointGrant, 0);
    TestTrue(TEXT("Carry purchases as Swift"), Progression->PurchaseNode(Kinetic, TEXT("Swift.Kinetic.Carry"), Failure));
    TestEqual(TEXT("Carry's slide speed is live for Swift"), Progression->GetNodeStats().SlideSpeedMultiplier, 1.12f, 0.0001f);

    // DevForceClass documents that it deliberately leaves a stale
    // ClassDefinition in place (still pointing at Swift's trees); the fix
    // must not depend on that pointer ever being refreshed.
    Progression->DevForceClass(EBreakerClassId::Caster);
    TestEqual(TEXT("The Swift rank stops paying once the permanent class changes"), Progression->GetNodeStats().SlideSpeedMultiplier, 1.0f, 0.0001f);

    // The rank itself is not deleted -- only its contribution stops. Swapping
    // back to Swift resumes it with no re-purchase.
    TestEqual(TEXT("The rank itself survives the swap"), Progression->GetNodeRank(TEXT("Swift.Kinetic.Carry"), EBreakerPointCurrency::DoctrinePoints), 1);
    Progression->DevForceClass(EBreakerClassId::Swift);
    TestEqual(TEXT("Swapping back resumes the contribution"), Progression->GetNodeStats().SlideSpeedMultiplier, 1.12f, 0.0001f);
    return true;
}

// ---------------------------------------------------------------------------
// ITEM 9 â€” subclass commitment (O37)
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
    // GRANTING MORE POINTS THAN THE GAME DOES HIDES REACHABILITY BUGS, and
    // this line is the reason six branch keystones sat unpurchasable for a
    // whole milestone with a green suite: the shipped grant was 10, the gate
    // was 8, a keystone cost 3, and every test that could have caught it
    // handed itself 30. The headroom is legitimate HERE â€” this test asserts
    // commitment semantics, not affordability, and it must be able to reach
    // the nodes it is testing. But any test asserting that content is
    // REACHABLE must run on the shipped entitlement, never on a grant, or it
    // proves the rule against a character the game cannot produce.
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
    TestTrue(TEXT("Committing to Frenzy succeeds"), Progression->CommitToBranch(TEXT("Doctrine.Swift.Frenzy"), CommitFailure));
    TestEqual(TEXT("The commitment is recorded"), Progression->GetProgressionState().CommittedBranch, FName(TEXT("Doctrine.Swift.Frenzy")));
    TestFalse(TEXT("A second commitment is refused, even to the SAME branch"),
        Progression->CommitToBranch(TEXT("Doctrine.Swift.Frenzy"), CommitFailure));
    TestFalse(TEXT("A second commitment to a DIFFERENT branch is also refused"),
        Progression->CommitToBranch(TEXT("Doctrine.Swift.Kinetic"), CommitFailure));
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
    TestTrue(TEXT("Class respec at a Forge succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, Failure));
    TestEqual(TEXT("Respec clears the commitment"), Progression->GetProgressionState().CommittedBranch, FName(NAME_None));
    TestEqual(TEXT("Respec also clears Bloodrhythm's rank"), Progression->GetNodeRank(TEXT("Swift.Frenzy.Bloodrhythm"), EBreakerPointCurrency::DoctrinePoints), 0);

    // A fresh commitment is possible again after the Forge visit.
    TestTrue(TEXT("A new commitment succeeds after the respec"), Progression->CommitToBranch(TEXT("Doctrine.Swift.Kinetic"), CommitFailure));
    return true;
}

// ---------------------------------------------------------------------------
// ITEM 10 â€” O39 auto-lock becomes gated (default unchanged)
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
        DefaultProgression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), UBreakerProgressionLibrary::SliceCorePointGrant);

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
        GatedProgression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), UBreakerProgressionLibrary::SliceCorePointGrant);

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
