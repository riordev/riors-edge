#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"

// These tests pin the invariants of the unified attribute application path.
// The bug they exist to prevent: equipment and progression each cached their
// own "base" attribute value and wrote an absolute result, so whichever layer
// recalculated last erased the other's contribution and gear affixes never
// stacked with skill nodes.

namespace BreakerAggregationTestHelpers
{
    // A component only counts itself authoritative through an owning actor,
    // and a freshly constructed actor is ROLE_Authority — enough to exercise
    // the server paths without a world.
    AActor* MakeOwner()
    {
        return NewObject<AActor>();
    }

    FBreakerRolledAffix MakeRolled(FName AffixId, float Value, EBreakerAffixCategory Category)
    {
        FBreakerRolledAffix Rolled;
        Rolled.AffixId = AffixId;
        Rolled.Tier = 4;
        Rolled.Value = Value;
        Rolled.Category = Category;
        return Rolled;
    }

    // +150 flat health and +8% increased movement speed across two slots.
    void MakeTestGear(TArray<FBreakerItemInstance>& OutItems)
    {
        FBreakerItemInstance Boots;
        Boots.ItemId = FGuid::NewGuid();
        Boots.Slot = EBreakerEquipSlot::Boots;
        Boots.Affixes.Add(MakeRolled(TEXT("Core.Health"), 100.0f, EBreakerAffixCategory::Suffix));
        Boots.Affixes.Add(MakeRolled(TEXT("Core.MoveSpeed"), 5.0f, EBreakerAffixCategory::Prefix));

        FBreakerItemInstance Helmet;
        Helmet.ItemId = FGuid::NewGuid();
        Helmet.Slot = EBreakerEquipSlot::Helmet;
        Helmet.Affixes.Add(MakeRolled(TEXT("Core.Health"), 50.0f, EBreakerAffixCategory::Suffix));
        Helmet.Affixes.Add(MakeRolled(TEXT("Core.MoveSpeed"), 3.0f, EBreakerAffixCategory::Prefix));

        OutItems.Add(Boots);
        OutItems.Add(Helmet);
    }

    // Core.Bulwark.SetStance is +90 flat health; Core.Kinesis.LightFooting is
    // +12% increased movement speed.
    FBreakerProgressionState MakeTestNodes()
    {
        FBreakerProgressionState NodeState;
        NodeState.PermanentClass = EBreakerClassId::Swift;
        NodeState.CoreNodeRanks.Add({TEXT("Core.Bulwark.SetStance"), 1});
        NodeState.CoreNodeRanks.Add({TEXT("Core.Kinesis.LightFooting"), 1});
        return NodeState;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAttributeAggregatorMathTest,
    "RiorsEdge.Attributes.Aggregator.LockedRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAttributeAggregatorMathTest::RunTest(const FString& Parameters)
{
    FBreakerAttributeAggregator Aggregator;
    float Bases[FBreakerAttributeAggregator::AttributeCount] = {};
    Bases[static_cast<int32>(EBreakerAggregatedAttribute::MaxHealth)] = 100.0f;
    TestTrue(TEXT("First capture wins"), Aggregator.CaptureBases(Bases));

    // The base is owned once. A second capture — a second layer trying to
    // snapshot after the first has contributed — must be ignored outright.
    float PollutedBases[FBreakerAttributeAggregator::AttributeCount] = {};
    PollutedBases[static_cast<int32>(EBreakerAggregatedAttribute::MaxHealth)] = 999.0f;
    TestFalse(TEXT("Later captures are refused"), Aggregator.CaptureBases(PollutedBases));
    TestEqual(TEXT("Base is untouched by the refused capture"), Aggregator.GetBase(EBreakerAggregatedAttribute::MaxHealth), 100.0f);

    FBreakerAttributeContribution Gear;
    TestTrue(TEXT("A fresh contribution is the identity"), Gear.IsIdentity());
    Gear.AddFlat(EBreakerAggregatedAttribute::MaxHealth, 150.0f);
    Gear.AddIncreasedPercent(EBreakerAggregatedAttribute::MaxHealth, 10.0f);

    FBreakerAttributeContribution Tree;
    Tree.AddFlat(EBreakerAggregatedAttribute::MaxHealth, 50.0f);
    Tree.AddIncreasedPercent(EBreakerAggregatedAttribute::MaxHealth, 20.0f);

    Aggregator.SetContribution(EBreakerAttributeContributor::Equipment, Gear);
    Aggregator.SetContribution(EBreakerAttributeContributor::Progression, Tree);

    // Flat sums FIRST, then all Increased percentages as ONE additive bucket:
    // (100 + 150 + 50) * (1 + 0.30) = 390. Multiplying the two Increased
    // sources instead (1.10 * 1.20) would give 396 — that is the failure this
    // pins.
    TestEqual(TEXT("Flat sums first, Increased is one additive bucket"),
        Aggregator.Compose(EBreakerAggregatedAttribute::MaxHealth), 390.0f, 0.001f);

    // More multipliers compose multiplicatively, on top of the additive bucket
    // and against each other. Reserved for trees and Anomalous items (O3).
    Gear.ComposeMore(EBreakerAggregatedAttribute::MaxHealth, 1.5f);
    Tree.ComposeMore(EBreakerAggregatedAttribute::MaxHealth, 1.2f);
    Aggregator.SetContribution(EBreakerAttributeContributor::Equipment, Gear);
    Aggregator.SetContribution(EBreakerAttributeContributor::Progression, Tree);
    TestEqual(TEXT("More multipliers compose multiplicatively"),
        Aggregator.Compose(EBreakerAggregatedAttribute::MaxHealth), 390.0f * 1.5f * 1.2f, 0.001f);

    // Two More multipliers on one contributor compose into its own product.
    FBreakerAttributeContribution Stacked;
    Stacked.ComposeMore(EBreakerAggregatedAttribute::MoveSpeed, 1.5f);
    Stacked.ComposeMore(EBreakerAggregatedAttribute::MoveSpeed, 1.2f);
    TestEqual(TEXT("A contributor's own More multipliers compose"),
        Stacked.GetMore(EBreakerAggregatedAttribute::MoveSpeed), 1.8f, 0.0001f);

    // Clearing is exact: back to the untouched base, not "base minus what we
    // think we added".
    Aggregator.ClearContribution(EBreakerAttributeContributor::Equipment);
    Aggregator.ClearContribution(EBreakerAttributeContributor::Progression);
    TestEqual(TEXT("Clearing every contribution restores the base exactly"),
        Aggregator.Compose(EBreakerAggregatedAttribute::MaxHealth), 100.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerGearAndNodesStackTest,
    "RiorsEdge.Attributes.Unified.GearAndNodesStack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerGearAndNodesStackTest::RunTest(const FString& Parameters)
{
    using namespace BreakerAggregationTestHelpers;

    TArray<FBreakerItemInstance> Gear;
    MakeTestGear(Gear);
    const FBreakerProgressionState Nodes = MakeTestNodes();

    // Sequence A: gear first, then nodes.
    UBreakerAttributeSet* AttributesA = NewObject<UBreakerAttributeSet>();
    AActor* OwnerA = MakeOwner();
    UBreakerEquipmentComponent* EquipmentA = NewObject<UBreakerEquipmentComponent>(OwnerA);
    UBreakerProgressionComponent* ProgressionA = NewObject<UBreakerProgressionComponent>(OwnerA);
    EquipmentA->BindAttributes(AttributesA);
    for (const FBreakerItemInstance& Item : Gear) EquipmentA->EquipItem(Item);
    ProgressionA->BindAttributes(AttributesA);
    ProgressionA->LoadProgressionState(Nodes);

    // Sequence B: nodes first, then gear.
    UBreakerAttributeSet* AttributesB = NewObject<UBreakerAttributeSet>();
    AActor* OwnerB = MakeOwner();
    UBreakerEquipmentComponent* EquipmentB = NewObject<UBreakerEquipmentComponent>(OwnerB);
    UBreakerProgressionComponent* ProgressionB = NewObject<UBreakerProgressionComponent>(OwnerB);
    ProgressionB->BindAttributes(AttributesB);
    ProgressionB->LoadProgressionState(Nodes);
    EquipmentB->BindAttributes(AttributesB);
    for (const FBreakerItemInstance& Item : Gear) EquipmentB->EquipItem(Item);

    // Both layers land: 100 base + 150 gear + 90 node. Under the old
    // last-recalc-wins bug this read 250 or 190.
    TestEqual(TEXT("Flat health from gear and nodes both apply"), AttributesA->GetMaxHealth(), 340.0f);
    TestEqual(TEXT("Health rides the new maximum"), AttributesA->GetHealth(), 340.0f);

    // 8% gear + 12% node in ONE additive bucket: 650 * 1.20 = 780. A
    // multiplicative composition would read 786.24, a last-writer-wins one
    // 702 or 728.
    TestEqual(TEXT("Increased move speed is one additive bucket across layers"), AttributesA->GetMoveSpeed(), 780.0f, 0.001f);

    // Order cannot matter.
    TestEqual(TEXT("Gear-then-node and node-then-gear agree on health"), AttributesB->GetMaxHealth(), AttributesA->GetMaxHealth());
    TestEqual(TEXT("Gear-then-node and node-then-gear agree on move speed"), AttributesB->GetMoveSpeed(), AttributesA->GetMoveSpeed());
    TestEqual(TEXT("Gear-then-node and node-then-gear agree on crit chance"), AttributesB->GetCriticalChance(), AttributesA->GetCriticalChance());
    TestEqual(TEXT("Gear-then-node and node-then-gear agree on crit multiplier"), AttributesB->GetCriticalMultiplier(), AttributesA->GetCriticalMultiplier());

    // The base is still the authored one, not a polluted snapshot.
    TestEqual(TEXT("The true base survives both layers"), AttributesA->GetAttributeBase(EBreakerAggregatedAttribute::MaxHealth), 100.0f);
    TestEqual(TEXT("The true move speed base survives both layers"), AttributesA->GetAttributeBase(EBreakerAggregatedAttribute::MoveSpeed), 650.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerUnifiedRemovalTest,
    "RiorsEdge.Attributes.Unified.RemovalIsExact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerUnifiedRemovalTest::RunTest(const FString& Parameters)
{
    using namespace BreakerAggregationTestHelpers;

    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    AActor* Owner = MakeOwner();
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    Equipment->BindAttributes(Attributes);
    Progression->BindAttributes(Attributes);

    // Nodes stay allocated throughout: removing gear must not disturb them.
    Progression->LoadProgressionState(MakeTestNodes());
    const float HealthWithNodesOnly = Attributes->GetMaxHealth();
    const float MoveSpeedWithNodesOnly = Attributes->GetMoveSpeed();
    TestEqual(TEXT("Nodes alone move health"), HealthWithNodesOnly, 190.0f);

    TArray<FBreakerItemInstance> Gear;
    MakeTestGear(Gear);
    for (const FBreakerItemInstance& Item : Gear) Equipment->EquipItem(Item);
    TestEqual(TEXT("Gear stacks on top of the nodes"), Attributes->GetMaxHealth(), 340.0f);

    Equipment->UnequipSlot(EBreakerEquipSlot::Boots);
    Equipment->UnequipSlot(EBreakerEquipSlot::Helmet);
    TestEqual(TEXT("Unequipping restores the pre-equip health exactly"), Attributes->GetMaxHealth(), HealthWithNodesOnly);
    TestEqual(TEXT("Unequipping restores the pre-equip move speed exactly"), Attributes->GetMoveSpeed(), MoveSpeedWithNodesOnly);

    // Repeated cycles must not drift by a single float.
    for (int32 Cycle = 0; Cycle < 50; ++Cycle)
    {
        for (const FBreakerItemInstance& Item : Gear) Equipment->EquipItem(Item);
        Equipment->UnequipSlot(EBreakerEquipSlot::Boots);
        Equipment->UnequipSlot(EBreakerEquipSlot::Helmet);
    }
    TestEqual(TEXT("Fifty equip/unequip cycles do not drift health"), Attributes->GetMaxHealth(), HealthWithNodesOnly);
    TestEqual(TEXT("Fifty equip/unequip cycles do not drift move speed"), Attributes->GetMoveSpeed(), MoveSpeedWithNodesOnly);

    // And a respec restores the true base, with gear still equipped.
    for (const FBreakerItemInstance& Item : Gear) Equipment->EquipItem(Item);
    FText Failure;
    TestTrue(TEXT("Respec at a Forge succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::CorePoints, true, Failure));
    TestEqual(TEXT("Respec leaves exactly the gear contribution"), Attributes->GetMaxHealth(), 250.0f);
    TestEqual(TEXT("Respec leaves exactly the gear move speed"), Attributes->GetMoveSpeed(), 650.0f * 1.08f, 0.001f);

    Equipment->UnequipSlot(EBreakerEquipSlot::Boots);
    Equipment->UnequipSlot(EBreakerEquipSlot::Helmet);
    TestEqual(TEXT("With nothing contributing, health is the authored base"), Attributes->GetMaxHealth(), 100.0f);
    TestEqual(TEXT("With nothing contributing, move speed is the authored base"), Attributes->GetMoveSpeed(), 650.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerClassResourceFloorTest,
    "RiorsEdge.Attributes.ClassResourceFloor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerClassResourceFloorTest::RunTest(const FString& Parameters)
{
    using namespace BreakerAggregationTestHelpers;

    // Spec D8. PreAttributeChange is called by name here because that is
    // literally what GAS calls on every attribute write, including the instant
    // GameplayEffect an ability cost applies (FGameplayAttribute::
    // SetNumericValueChecked). Testing it directly tests the real cost path.
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    TestEqual(TEXT("Every character starts with a closed floor"), Attributes->GetClassResourceFloor(), 0.0f);

    // THE REGRESSION GUARD: with a closed floor the clamp is the old [0, Max].
    float Value = -45.0f;
    Attributes->PreAttributeChange(UBreakerAttributeSet::GetClassResourceAttribute(), Value);
    TestEqual(TEXT("A closed floor clamps the bank at zero, exactly as before"), Value, 0.0f);
    Value = 250.0f;
    Attributes->PreAttributeChange(UBreakerAttributeSet::GetClassResourceAttribute(), Value);
    TestEqual(TEXT("A closed floor still clamps at the maximum"), Value, 100.0f);

    // Caster opens it. This is the single line that makes Overcast reachable.
    Attributes->ApplyClassResourceFloor(-20.0f);
    TestEqual(TEXT("The floor opens to the Overcast depth"), Attributes->GetClassResourceFloor(), -20.0f);

    Value = -15.0f;
    Attributes->PreAttributeChange(UBreakerAttributeSet::GetClassResourceAttribute(), Value);
    TestEqual(TEXT("A cost may now drive the bank negative"), Value, -15.0f);
    Value = -20.0f;
    Attributes->PreAttributeChange(UBreakerAttributeSet::GetClassResourceAttribute(), Value);
    TestEqual(TEXT("A cost may land exactly on the floor"), Value, -20.0f);
    Value = -45.0f;
    Attributes->PreAttributeChange(UBreakerAttributeSet::GetClassResourceAttribute(), Value);
    TestEqual(TEXT("Nothing drives the bank past the floor"), Value, -20.0f);

    // A positive floor would mean "this bank may never be emptied".
    Attributes->ApplyClassResourceFloor(25.0f);
    TestEqual(TEXT("A positive floor is refused and reads as closed"), Attributes->GetClassResourceFloor(), 0.0f);

    // Closing the floor must lift a bank that is now stranded below it: a
    // respec or class change cannot leave a character in permanent debt.
    Attributes->ApplyClassResourceFloor(-20.0f);
    Attributes->ApplyClassResource(-18.0f);
    TestEqual(TEXT("The bank sits in debt"), Attributes->GetClassResource(), -18.0f);
    Attributes->ApplyClassResourceFloor(0.0f);
    TestEqual(TEXT("Closing the floor lifts the stranded bank to zero"), Attributes->GetClassResource(), 0.0f);
    TestEqual(TEXT("Closing the floor closes it"), Attributes->GetClassResourceFloor(), 0.0f);

    // The floor is NOT an aggregated attribute: no gear affix and no skill node
    // bids on it, and a fold of (Base + flat) * (1 + Increased) over a base of
    // zero would annihilate every percentage anyway. Prove the aggregation pass
    // leaves it — and a bank sitting in debt — completely alone.
    Attributes->ApplyClassResourceFloor(-20.0f);
    Attributes->ApplyClassResource(-12.0f);

    AActor* Owner = MakeOwner();
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    Equipment->BindAttributes(Attributes);
    Progression->BindAttributes(Attributes);
    TArray<FBreakerItemInstance> Gear;
    MakeTestGear(Gear);
    for (const FBreakerItemInstance& Item : Gear) Equipment->EquipItem(Item);
    Progression->LoadProgressionState(MakeTestNodes());

    TestEqual(TEXT("Equipping gear and allocating nodes does not touch the floor"), Attributes->GetClassResourceFloor(), -20.0f);
    TestEqual(TEXT("A recalculation does not silently repay the debt"), Attributes->GetClassResource(), -12.0f);
    TestEqual(TEXT("The aggregated attributes still fold as before"), Attributes->GetMaxHealth(), 340.0f);

    FText Failure;
    Progression->RespecAtForge(EBreakerPointCurrency::CorePoints, true, Failure);
    TestEqual(TEXT("A respec does not touch the floor either"), Attributes->GetClassResourceFloor(), -20.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDamageNodeRaisesWeaponDamageTest,
    "RiorsEdge.Attributes.Damage.NodePurchaseRaisesWeaponDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDamageNodeRaisesWeaponDamageTest::RunTest(const FString& Parameters)
{
    using namespace BreakerAggregationTestHelpers;

    // THE owner-reported bug: "no matter what I do damage still feels the
    // same". EBreakerNodeStatTarget had no damage entry and nothing ever wrote
    // the DamageMultiplier attribute, so a purchased node could not move the
    // number a weapon deals. This test buys a node and checks the damage.

    // Exactly the composition UBreakerWeaponComponent performs per pellet:
    // BaseDamage * the DamageMultiplier attribute, resolved through the shared
    // damage library. Criticals are off so the assertion is deterministic.
    auto WeaponDamageFor = [](const UBreakerAttributeSet* Attributes)
    {
        FBreakerDamageRequest Request;
        Request.BaseDamage = 100.0f;
        Request.bCanCritical = false;
        Request.SourceDamageMultiplier = Attributes->GetDamageMultiplier();
        FBreakerDefenseState Defense;
        Defense.Health = 100000.0f;
        return UBreakerDamageLibrary::ResolveDamage(Request, Defense).HealthDamage;
    };

    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    AActor* Owner = MakeOwner();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    // Isolate the node effect from the point-spend baseline for this first
    // assertion; the baseline gets its own test below.
    Progression->IncreasedDamagePerSpentPoint = 0.0f;
    Progression->BindAttributes(Attributes);

    TestEqual(TEXT("An unspent character deals exactly base damage"), WeaponDamageFor(Attributes), 100.0f, 0.001f);
    TestEqual(TEXT("The damage multiplier starts neutral"), Attributes->GetDamageMultiplier(), 1.0f, 0.0001f);

    UBreakerProgressionTree* Core = UBreakerProgressionLibrary::GetCoreSliceTree();
    Progression->ApplySliceDefaultsIfFresh();
    FText Failure;
    // ATLAS SHAPE (Phase 4): single purchases summing in the one bucket —
    // reached THROUGH THE RING: entry at Sightline, the run to VOLLEY walked
    // on Ability picks, which never touch the DamageMultiplier attribute
    // this test is watching.
    TestTrue(TEXT("The entry purchases"), Progression->PurchaseNode(Core, TEXT("Core.Precision.Sightline"), Failure));
    TestTrue(TEXT("Travel 1 purchases"), Progression->PurchaseNode(Core, TEXT("Core.Travel.Ring0P1Ability"), Failure));
    TestTrue(TEXT("Travel 2 purchases"), Progression->PurchaseNode(Core, TEXT("Core.Travel.Ring0P2Ability"), Failure));
    TestTrue(TEXT("Travel 3 purchases"), Progression->PurchaseNode(Core, TEXT("Core.Travel.Ring0P3Ability"), Failure));
    TestEqual(TEXT("The path moved no weapon damage"), Attributes->GetDamageMultiplier(), 1.0f, 0.0001f);
    TestTrue(TEXT("The damage rim purchases"), Progression->PurchaseNode(Core, TEXT("Core.Volley.Cyclic"), Failure));

    TestEqual(TEXT("A purchased damage node moves the attribute"), Attributes->GetDamageMultiplier(), 1.03f, 0.0001f);
    TestEqual(TEXT("A purchased damage node moves the damage a weapon would deal"), WeaponDamageFor(Attributes), 103.0f, 0.001f);

    // Separate purchases keep paying into ONE additive bucket, walked in
    // ring order — Feed opens off Cyclic, Trigger Discipline off Feed, and
    // Salvo's stated AND gate (Cyclic + Feed) is met when adjacency reaches it.
    TestTrue(TEXT("A shared-pool rim purchases"), Progression->PurchaseNode(Core, TEXT("Core.Volley.Feed"), Failure));
    TestEqual(TEXT("The shared rim adds its increment"), Attributes->GetDamageMultiplier(), 1.05f, 0.0001f);
    TestTrue(TEXT("A second weapon rim purchases"), Progression->PurchaseNode(Core, TEXT("Core.Volley.TriggerDiscipline"), Failure));
    TestTrue(TEXT("The rim-gated inner purchases"), Progression->PurchaseNode(Core, TEXT("Core.Volley.Salvo"), Failure));
    TestEqual(TEXT("Purchases sum into the one additive bucket"), Attributes->GetDamageMultiplier(), 1.24f, 0.0001f);
    TestEqual(TEXT("The wheel purchases are worth 24% more damage"), WeaponDamageFor(Attributes), 124.0f, 0.001f);

    // A respec must hand the damage back exactly, not approximately.
    TestTrue(TEXT("Respec at a Forge succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::CorePoints, true, Failure));
    TestEqual(TEXT("Respec restores the neutral multiplier exactly"), Attributes->GetDamageMultiplier(), 1.0f, 0.0001f);
    TestEqual(TEXT("Respec restores base weapon damage exactly"), WeaponDamageFor(Attributes), 100.0f, 0.001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerPointSpendDamageBaselineTest,
    "RiorsEdge.Attributes.Damage.PointSpendBaseline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPointSpendDamageBaselineTest::RunTest(const FString& Parameters)
{
    using namespace BreakerAggregationTestHelpers;

    // The owner asked for damage to rise with spending points, not only on the
    // nodes that happen to author damage. Every committed point pays a small
    // Increased Damage baseline into the same additive bucket.
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    AActor* Owner = MakeOwner();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    Progression->BindAttributes(Attributes);
    // Cut from 1.0% to 0.25% under O27: at 1.0% this out-earned every damage
    // node in the game by about 3.5x, so how MANY points had been spent
    // mattered more than where. It is a floor now, not a power source.
    TestEqual(TEXT("The baseline defaults to 0.25% per point"), Progression->IncreasedDamagePerSpentPoint, 0.25f, 0.0001f);
    TestEqual(TEXT("Nothing spent pays nothing"), Progression->GetPointSpendDamagePercent(), 0.0f, 0.0001f);

    UBreakerProgressionTree* Core = UBreakerProgressionLibrary::GetCoreSliceTree();
    Progression->ApplySliceDefaultsIfFresh();
    FText Failure;

    // Core.Bulwark.SetStance costs 1 and authors NO damage effect at all: the
    // whole point is that spending anywhere is felt.
    TestTrue(TEXT("A node with no damage effect purchases"), Progression->PurchaseNode(Core, TEXT("Core.Bulwark.SetStance"), Failure));
    TestEqual(TEXT("One committed point is one point"), Progression->GetSpentPoints(), 1.0f, 0.0001f);
    TestEqual(TEXT("A damage-less purchase still raises damage"), Attributes->GetDamageMultiplier(), 1.0025f, 0.0001f);

    // Cost, not node count, is the unit: a 2-point inner is worth two. The
    // ring is walked inside BULWARK, whose entry SetStance is already owned:
    // Read and Weight ride the hexagon, Held Ground completes
    // Counterweight's stated AND gate.
    TestTrue(TEXT("A weapon rim purchases"), Progression->PurchaseNode(Core, TEXT("Core.Bulwark.Read"), Failure));
    TestTrue(TEXT("A damage rim purchases"), Progression->PurchaseNode(Core, TEXT("Core.Bulwark.Weight"), Failure));
    TestTrue(TEXT("A second damage rim purchases"), Progression->PurchaseNode(Core, TEXT("Core.Bulwark.HeldGround"), Failure));
    TestTrue(TEXT("The 2-point inner purchases"), Progression->PurchaseNode(Core, TEXT("Core.Bulwark.Counterweight"), Failure));
    TestEqual(TEXT("Committed points count by cost"), Progression->GetSpentPoints(), 6.0f, 0.0001f);
    // 6 points x 0.25% baseline + the node lines (3 + 2 + 2 + 16), all one
    // bucket. The nodes out-earn the accumulation by an order of magnitude;
    // before the O27 cut they were equal, which is the imbalance it names.
    TestEqual(TEXT("Baseline and node damage share one additive bucket"), Attributes->GetDamageMultiplier(), 1.245f, 0.0001f);

    // Turning the baseline off must leave exactly the node content behind, so
    // the owner can retune or disable it without touching content.
    const FBreakerProgressionState Allocated = Progression->GetProgressionState();
    Progression->IncreasedDamagePerSpentPoint = 0.0f;
    Progression->LoadProgressionState(Allocated);
    TestEqual(TEXT("A zeroed baseline leaves only node damage"), Attributes->GetDamageMultiplier(), 1.23f, 0.0001f);

    Progression->IncreasedDamagePerSpentPoint = 2.5f;
    Progression->LoadProgressionState(Allocated);
    TestEqual(TEXT("The baseline retunes without a content change"), Attributes->GetDamageMultiplier(), 1.38f, 0.0001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerGearAndTreeDamageShareOneBucketTest,
    "RiorsEdge.Attributes.Damage.GearAndTreeShareOneBucket",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerGearAndTreeDamageShareOneBucketTest::RunTest(const FString& Parameters)
{
    using namespace BreakerAggregationTestHelpers;

    // Gear's Weapon Damage affix used to reach the weapon on a private path
    // and be MULTIPLIED against the attribute, which would have made gear and
    // tree damage compose multiplicatively — the locked rule says one additive
    // Increased bucket per stat. This pins the composition.
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    AActor* Owner = MakeOwner();
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    Progression->IncreasedDamagePerSpentPoint = 0.0f;
    Equipment->BindAttributes(Attributes);
    Progression->BindAttributes(Attributes);

    FBreakerItemInstance Gloves;
    Gloves.ItemId = FGuid::NewGuid();
    Gloves.Slot = EBreakerEquipSlot::Gloves;
    Gloves.Affixes.Add(MakeRolled(TEXT("Offense.WeaponDamage"), 12.0f, EBreakerAffixCategory::Prefix));

    FBreakerItemInstance Necklace;
    Necklace.ItemId = FGuid::NewGuid();
    Necklace.Slot = EBreakerEquipSlot::Necklace;
    Necklace.Affixes.Add(MakeRolled(TEXT("Offense.WeaponDamage"), 8.0f, EBreakerAffixCategory::Prefix));

    Equipment->EquipItem(Gloves);
    Equipment->EquipItem(Necklace);
    TestEqual(TEXT("Gear damage reaches the attribute at all"), Attributes->GetDamageMultiplier(), 1.20f, 0.0001f);
    TestEqual(TEXT("The gear-only display figure is unchanged"), Equipment->GetStats().WeaponDamageMultiplier, 1.20f, 0.0001f);

    FBreakerProgressionState Nodes;
    Nodes.PermanentClass = EBreakerClassId::Swift;
    Nodes.CoreNodeRanks.Add({TEXT("Core.Volley.Cyclic"), 1});  // +3% weapon
    Nodes.CoreNodeRanks.Add({TEXT("Core.Volley.Feed"), 1});    // +2% shared
    Nodes.CoreNodeRanks.Add({TEXT("Core.Volley.Salvo"), 1});   // +16% weapon
    Progression->LoadProgressionState(Nodes);

    // 20% gear + 21% tree in ONE bucket = 1.41. Multiplying the layers would
    // read 1.452; that gap is the whole bug class.
    TestEqual(TEXT("Gear and tree damage sum, never multiply"), Attributes->GetDamageMultiplier(), 1.41f, 0.0001f);

    // And removal is exact in both directions.
    Equipment->UnequipSlot(EBreakerEquipSlot::Gloves);
    Equipment->UnequipSlot(EBreakerEquipSlot::Necklace);
    TestEqual(TEXT("Unequipping leaves exactly the tree damage"), Attributes->GetDamageMultiplier(), 1.21f, 0.0001f);
    FText Failure;
    Progression->RespecAtForge(EBreakerPointCurrency::CorePoints, true, Failure);
    TestEqual(TEXT("A respec restores the true base of 1.0"), Attributes->GetDamageMultiplier(), 1.0f, 0.0001f);
    TestEqual(TEXT("The damage base was never polluted"), Attributes->GetAttributeBase(EBreakerAggregatedAttribute::DamageMultiplier), 1.0f, 0.0001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerResourceAffixesReachConsumersTest,
    "RiorsEdge.Attributes.Affixes.ResourceAffixesAreLive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerResourceAffixesReachConsumersTest::RunTest(const FString& Parameters)
{
    using namespace BreakerAggregationTestHelpers;

    // Core.MaxResource and Core.ResourceRegen were suspected dead. They are
    // not: MaxResource is a flat bid on the MaxClassResource attribute (which
    // Momentum and Mana both clamp against and the HUD bar reads as a
    // fraction), and ResourceRegen is ticked by the equipment component itself.
    // This test exists so a future refactor that quietly drops either one
    // fails loudly instead of shipping an affix that lies to the player.
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    AActor* Owner = MakeOwner();
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);
    Equipment->BindAttributes(Attributes);
    const float BaseMaxResource = Attributes->GetMaxClassResource();

    FBreakerItemInstance Waist;
    Waist.ItemId = FGuid::NewGuid();
    Waist.Slot = EBreakerEquipSlot::Waist;
    Waist.Affixes.Add(MakeRolled(TEXT("Core.MaxResource"), 30.0f, EBreakerAffixCategory::Suffix));
    Waist.Affixes.Add(MakeRolled(TEXT("Core.ResourceRegen"), 2.0f, EBreakerAffixCategory::Suffix));
    Equipment->EquipItem(Waist);

    TestEqual(TEXT("Maximum Resource raises the attribute the class loops clamp against"),
        Attributes->GetMaxClassResource(), BaseMaxResource + 30.0f, 0.0001f);
    TestEqual(TEXT("Resource Regeneration is aggregated for the equipment tick"),
        Equipment->GetStats().ResourceRegenPerSecond, 2.0f, 0.0001f);

    Equipment->UnequipSlot(EBreakerEquipSlot::Waist);
    TestEqual(TEXT("Removing the piece restores the base maximum exactly"), Attributes->GetMaxClassResource(), BaseMaxResource, 0.0001f);
    TestEqual(TEXT("Removing the piece stops the regeneration"), Equipment->GetStats().ResourceRegenPerSecond, 0.0f, 0.0001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerUnifiedReloadConvergenceTest,
    "RiorsEdge.Attributes.Unified.ReloadConverges",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerUnifiedReloadConvergenceTest::RunTest(const FString& Parameters)
{
    using namespace BreakerAggregationTestHelpers;

    TArray<FBreakerItemInstance> Gear;
    MakeTestGear(Gear);
    const FBreakerProgressionState Nodes = MakeTestNodes();

    // Incremental play: equip, allocate, equip again.
    UBreakerAttributeSet* Played = NewObject<UBreakerAttributeSet>();
    AActor* PlayedOwner = MakeOwner();
    UBreakerEquipmentComponent* PlayedEquipment = NewObject<UBreakerEquipmentComponent>(PlayedOwner);
    UBreakerProgressionComponent* PlayedProgression = NewObject<UBreakerProgressionComponent>(PlayedOwner);
    PlayedEquipment->BindAttributes(Played);
    PlayedProgression->BindAttributes(Played);
    PlayedEquipment->EquipItem(Gear[0]);
    PlayedProgression->LoadProgressionState(Nodes);
    PlayedEquipment->EquipItem(Gear[1]);

    // Save/load: both containers restored wholesale, in one shot.
    UBreakerAttributeSet* Loaded = NewObject<UBreakerAttributeSet>();
    AActor* LoadedOwner = MakeOwner();
    UBreakerEquipmentComponent* LoadedEquipment = NewObject<UBreakerEquipmentComponent>(LoadedOwner);
    UBreakerProgressionComponent* LoadedProgression = NewObject<UBreakerProgressionComponent>(LoadedOwner);
    LoadedEquipment->BindAttributes(Loaded);
    LoadedProgression->BindAttributes(Loaded);
    LoadedEquipment->RestoreState(Gear, {});
    LoadedProgression->LoadProgressionState(Nodes);

    TestEqual(TEXT("A loaded save converges on the played health"), Loaded->GetMaxHealth(), Played->GetMaxHealth());
    TestEqual(TEXT("A loaded save converges on the played move speed"), Loaded->GetMoveSpeed(), Played->GetMoveSpeed());
    TestEqual(TEXT("A loaded save converges on the played crit chance"), Loaded->GetCriticalChance(), Played->GetCriticalChance());
    TestEqual(TEXT("A loaded save converges on the played DoT multiplier"), Loaded->GetDamageOverTimeMultiplier(), Played->GetDamageOverTimeMultiplier());
    return true;
}

#endif
