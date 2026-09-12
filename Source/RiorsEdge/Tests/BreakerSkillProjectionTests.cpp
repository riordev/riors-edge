#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionNode.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerItemTypes.h"
#include "UI/BreakerSkillProjection.h"

// The skill screen prints "1.13x -> 1.16x" next to a node. These tests exist
// so that arrow cannot lie: the projection has to agree with the live
// aggregation both before the purchase (it mirrors the component's own offer)
// and after it (a real purchase lands exactly where the arrow pointed).

namespace BreakerSkillProjectionTestHelpers
{
    struct FRig
    {
        AActor* Owner = nullptr;
        UBreakerAttributeSet* Attributes = nullptr;
        UBreakerProgressionComponent* Progression = nullptr;
    };

    FRig MakeRig()
    {
        FRig Rig;
        Rig.Owner = NewObject<AActor>();
        Rig.Attributes = NewObject<UBreakerAttributeSet>();
        Rig.Progression = NewObject<UBreakerProgressionComponent>(Rig.Owner);
        Rig.Progression->BindAttributes(Rig.Attributes);
        Rig.Progression->ChoosePermanentClassById(EBreakerClassId::Swift);
        Rig.Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(12, Rig.Progression->ExperienceCurve));
        return Rig;
    }

    const UBreakerProgressionTree* CoreTree()
    {
        return UBreakerProgressionLibrary::GetCoreSliceTree();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSkillProjectionMirrorTest,
    "RiorsEdge.UI.SkillProjection.MirrorsLiveComponent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSkillProjectionMirrorTest::RunTest(const FString& Parameters)
{
    using namespace BreakerSkillProjectionTestHelpers;

    FRig Rig = MakeRig();
    FText Reason;
    // A legal gateway and two lane entries provide owned content for the mirror.
    for (const TCHAR* Id : {TEXT("Core.Precision.Sightline"), TEXT("Core.Precision.Angle"), TEXT("Core.Precision.Ledger")})
        if (!TestTrue(Id, Rig.Progression->PurchaseNode(CoreTree(), Id, Reason))) return false;
    const FBreakerSkillSnapshot Snapshot = BreakerSkillProjection::MakeSnapshot(Rig.Progression, Rig.Attributes);
    TestTrue(TEXT("Snapshot sees composed attributes"), Snapshot.bHasComposedAttributes);
    TestTrue(TEXT("Snapshot gathered the fallback node content"), Snapshot.Nodes.Num() > 10);

    FBreakerAttributeContribution Rebuilt;
    const FBreakerNodeStats RebuiltStats = BreakerSkillProjection::BuildOffer(Snapshot, Snapshot.Ranks, Rebuilt);
    const FBreakerAttributeContribution& Live = Rig.Progression->GetAttributeContribution();

    // Every bucket of every aggregated attribute, not just the one the test
    // author happened to think of.
    for (int32 Index = 0; Index < FBreakerAttributeContribution::AttributeCount; ++Index)
    {
        const EBreakerAggregatedAttribute Attribute = static_cast<EBreakerAggregatedAttribute>(Index);
        TestEqual(*FString::Printf(TEXT("Flat bucket %d mirrors the live offer"), Index),
            Rebuilt.GetFlat(Attribute), Live.GetFlat(Attribute), 0.0001f);
        TestEqual(*FString::Printf(TEXT("Increased bucket %d mirrors the live offer"), Index),
            Rebuilt.GetIncreasedPercent(Attribute), Live.GetIncreasedPercent(Attribute), 0.0001f);
        TestEqual(*FString::Printf(TEXT("More bucket %d mirrors the live offer"), Index),
            Rebuilt.GetMore(Attribute), Live.GetMore(Attribute), 0.0001f);
    }

    const FBreakerNodeStats& LiveStats = Rig.Progression->GetNodeStats();
    TestEqual(TEXT("Damage multiplier mirrors the live node stats"), RebuiltStats.DamageMultiplier, LiveStats.DamageMultiplier, 0.0001f);
    TestEqual(TEXT("Move speed multiplier mirrors the live node stats"), RebuiltStats.MoveSpeedMultiplier, LiveStats.MoveSpeedMultiplier, 0.0001f);
    TestEqual(TEXT("Bonus health mirrors the live node stats"), RebuiltStats.BonusHealth, LiveStats.BonusHealth, 0.0001f);
    TestEqual(TEXT("Dodge bonus mirrors the live node stats"), RebuiltStats.DodgeChanceBonus, LiveStats.DodgeChanceBonus, 0.0001f);

    // And the totals row the rail prints is the attribute the game actually
    // rolls damage against.
    const TArray<FBreakerStatLine> Totals = BreakerSkillProjection::CurrentTotals(Snapshot);
    TestEqual(TEXT("Totals produce one row per stat"), Totals.Num(), BreakerSkillProjection::StatRowCount());
    TestEqual(TEXT("The weapon damage row is the composed DamageMultiplier attribute"),
        Totals[0].Before, Rig.Attributes->GetDamageMultiplier(), 0.0001f);
    TestFalse(TEXT("A totals row never claims a purchase"), Totals[0].Changed());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSkillProjectionPurchaseTest,
    "RiorsEdge.UI.SkillProjection.ProjectionMatchesRealPurchase",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSkillProjectionPurchaseTest::RunTest(const FString& Parameters)
{
    using namespace BreakerSkillProjectionTestHelpers;

    FRig Rig = MakeRig();
    FText Reason;
    const auto Snapshot = BreakerSkillProjection::MakeSnapshot(Rig.Progression, Rig.Attributes);
    const auto Gateway = BreakerSkillProjection::ProjectPurchase(Snapshot, TEXT("Core.Precision.Sightline"), 1);
    TestEqual(TEXT("Projection begins at live damage"), Gateway[0].Before, Rig.Attributes->GetDamageMultiplier(), .0001f);
    TestEqual(TEXT("Crit-damage gateway adds no damage for its point"), Gateway[0].After - Gateway[0].Before, 0.0f, .0001f);
    if (!TestTrue(TEXT("Gateway purchased"), Rig.Progression->PurchaseNode(CoreTree(), TEXT("Core.Precision.Sightline"), Reason))) return false;
    TestEqual(TEXT("Gateway lands at its projection"), Rig.Attributes->GetDamageMultiplier(), Gateway[0].After, .0001f);
    if (!TestTrue(TEXT("Rank-one lane opens notable"), Rig.Progression->PurchaseNode(CoreTree(), TEXT("Core.Precision.Angle"), Reason))) return false;
    const auto BeforeNotable = BreakerSkillProjection::MakeSnapshot(Rig.Progression, Rig.Attributes);
    const auto Notable = BreakerSkillProjection::ProjectPurchase(BeforeNotable, TEXT("Core.Precision.CalledShot"), 1);
    TestEqual(TEXT("Two-point notable adds twenty percent and nothing for its points"), Notable[0].After - Notable[0].Before, .20f, .0001f);
    if (!TestTrue(TEXT("Called Shot purchased"), Rig.Progression->PurchaseNode(CoreTree(), TEXT("Core.Precision.CalledShot"), Reason))) return false;
    TestEqual(TEXT("Notable lands at its projection"), Rig.Attributes->GetDamageMultiplier(), Notable[0].After, .0001f);
    if (!TestTrue(TEXT("Cadence lane purchased"), Rig.Progression->PurchaseNode(CoreTree(), TEXT("Core.Precision.Cadence"), Reason))) return false;
    const auto BeforeCrit = BreakerSkillProjection::MakeSnapshot(Rig.Progression, Rig.Attributes);
    const auto Crit = BreakerSkillProjection::ProjectPurchase(BeforeCrit, TEXT("Core.Precision.TriggerDiscipline"), 1);
    TestEqual(TEXT("Crit notable adds no damage for its points"), Crit[0].After - Crit[0].Before, 0.0f, .0001f);
    if (!TestTrue(TEXT("Trigger Discipline purchased"), Rig.Progression->PurchaseNode(CoreTree(), TEXT("Core.Precision.TriggerDiscipline"), Reason))) return false;
    TestEqual(TEXT("Non-Increased damage notable lands at projection"), Rig.Attributes->GetDamageMultiplier(), Crit[0].After, .0001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSkillProjectionArithmeticTest,
    "RiorsEdge.UI.SkillProjection.RankMathAndFormatting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSkillProjectionArithmeticTest::RunTest(const FString& Parameters)
{
    using namespace BreakerSkillProjection;

    TArray<FBreakerNodeRank> Ranks;
    Ranks.Add({TEXT("A"), 2});

    const TArray<FBreakerNodeRank> Raised = WithRankDelta(Ranks, TEXT("A"), 1);
    TestEqual(TEXT("An owned rank rises"), Raised[0].Rank, 3);
    TestEqual(TEXT("Raising an owned rank adds no entry"), Raised.Num(), 1);

    const TArray<FBreakerNodeRank> Added = WithRankDelta(Ranks, TEXT("B"), 1);
    TestEqual(TEXT("An unowned node gains an entry"), Added.Num(), 2);
    TestEqual(TEXT("The new entry starts at the delta"), Added[1].Rank, 1);

    const TArray<FBreakerNodeRank> Removed = WithRankDelta(Ranks, TEXT("A"), -5);
    TestEqual(TEXT("Ranks never go negative"), Removed[0].Rank, 0);
    TestEqual(TEXT("A zero delta changes nothing"), WithRankDelta(Ranks, TEXT("A"), 0).Num(), 1);
    TestEqual(TEXT("An unowned node is not created by a refund"), WithRankDelta(Ranks, TEXT("B"), -1).Num(), 1);

    // Costs, not ranks: a 3-point Convergence is worth three minors.
    TArray<const UBreakerProgressionNode*> Nodes;
    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
    {
        for (const UBreakerProgressionNode* Node : Tree->Nodes) Nodes.AddUnique(Node);
    }
    TArray<FBreakerNodeRank> Committed;
    Committed.Add({TEXT("Core.Precision.Fixate"), 1});          // cost 3
    Committed.Add({TEXT("Core.Precision.CalledShot"), 1});              // cost 2
    Committed.Add({TEXT("Core.Precision.Sightline"), 1});             // cost 1
    TestEqual(TEXT("Committed points count cost, not rank"), CommittedPoints(Nodes, Committed), 6);
    TestEqual(TEXT("An unknown node falls back to cost 1"),
        CommittedPoints(Nodes, {{TEXT("Nope"), 4}}), 4);

    FBreakerStatLine Line;
    Line.Format = EBreakerStatFormat::Multiplier;
    Line.Before = 1.13f;
    Line.After = 1.16f;
    TestEqual(TEXT("A multiplier reads as a total"), FormatStat(Line.After, Line.Format), FString(TEXT("1.16x")));
    TestEqual(TEXT("A multiplier transition reads as before-and-after"), FormatTransition(Line), FString(TEXT("1.13x -> 1.16x")));
    TestEqual(TEXT("A multiplier delta reads in whole percent"), FormatDelta(Line), FString(TEXT("+3%")));

    Line.Format = EBreakerStatFormat::PercentPoints;
    Line.Before = 0.12f;
    Line.After = 0.19f;
    TestEqual(TEXT("A chance reads as a percentage"), FormatStat(Line.After, Line.Format), FString(TEXT("19.0%")));
    TestEqual(TEXT("A chance delta reads in percentage points"), FormatDelta(Line), FString(TEXT("+7.0%")));

    Line.Format = EBreakerStatFormat::Absolute;
    Line.Before = 340.0f;
    Line.After = 430.0f;
    TestEqual(TEXT("An absolute reads with no unit"), FormatStat(Line.After, Line.Format), FString(TEXT("430")));
    TestEqual(TEXT("An absolute delta is signed"), FormatDelta(Line), FString(TEXT("+90")));

    Line.After = Line.Before;
    TestEqual(TEXT("An unchanged line has no delta text"), FormatDelta(Line), FString());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSkillProjectionGuardTest,
    "RiorsEdge.UI.SkillProjection.SurvivesNoProgression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSkillProjectionGuardTest::RunTest(const FString& Parameters)
{
    // The screen must survive no progression component, no class, and no tree
    // content; so must the arithmetic behind it.
    const FBreakerSkillSnapshot Empty = BreakerSkillProjection::MakeSnapshot(nullptr, nullptr);
    TestFalse(TEXT("An empty snapshot claims no composed attributes"), Empty.bHasComposedAttributes);
    const TArray<FBreakerStatLine> Totals = BreakerSkillProjection::CurrentTotals(Empty);
    TestEqual(TEXT("An empty snapshot still produces every row"), Totals.Num(), BreakerSkillProjection::StatRowCount());
    for (const FBreakerStatLine& Line : Totals)
    {
        TestFalse(TEXT("No row claims a change"), Line.Changed());
        TestTrue(TEXT("Every row is flagged tree-only with no attribute set"), Line.bTreeOnly);
    }
    TestEqual(TEXT("Damage rests at identity"), Totals[0].Before, 1.0f, 0.0001f);

    // A component with no class and no points is the other reachable guard.
    AActor* Owner = NewObject<AActor>();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    const FBreakerSkillSnapshot Fresh = BreakerSkillProjection::MakeSnapshot(Progression, nullptr);
    TestEqual(TEXT("A classless character holds no ranks"), Fresh.Ranks.Num(), 0);
    TestEqual(TEXT("A classless projection is identity"),
        BreakerSkillProjection::ProjectPurchase(Fresh, TEXT("Core.Precision.Sightline"), 1)[0].Before, 1.0f, 0.0001f);
    return true;
}


// ---------------------------------------------------------------------------
// WHAT EQUIPPING THIS WOULD DO
// ---------------------------------------------------------------------------
// The composed delta replaces the printed gear score, and the reason it can is
// that it reports PER LANE. These pin the two properties a scalar could not
// have: that an item touching one delivery lane moves only that lane, and that
// equipping REPLACES rather than adds, so a downgrade reads as a loss.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEquipDeltaTest,
    "RiorsEdge.UI.EquipDelta.PerLane",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEquipDeltaTest::RunTest(const FString& Parameters)
{
    FBreakerSkillSnapshot Snapshot;
    Snapshot.bHasComposedAttributes = true;
    // Both damage lanes are 1.0-based, so an empty aggregator composes to
    // identity and every movement below is the gear layer alone.
    TArray<float> Bases;
    Bases.SetNumZeroed(FBreakerAttributeContribution::AttributeCount);
    Bases[static_cast<int32>(EBreakerAggregatedAttribute::DamageMultiplier)] = 1.0f;
    Bases[static_cast<int32>(EBreakerAggregatedAttribute::AbilityDamageMultiplier)] = 1.0f;
    Bases[static_cast<int32>(EBreakerAggregatedAttribute::CriticalMultiplier)] = 1.0f;
    Bases[static_cast<int32>(EBreakerAggregatedAttribute::MaxHealth)] = 100.0f;
    for (int32 Index = 0; Index < Bases.Num(); ++Index)
    {
        Snapshot.Aggregator.SetBase(static_cast<EBreakerAggregatedAttribute>(Index), Bases[Index]);
    }

    // One real affix out of the shipped pool, on one slot. Nothing invented.
    const auto MakeItem = [](EBreakerEquipSlot Slot, const TCHAR* AffixId)
    {
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.Slot = Slot;
        Item.ItemLevel = 50;
        Item.Rarity = EBreakerItemRarity::Exceptional;
        const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
        if (const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, AffixId))
        {
            FBreakerRolledAffix Rolled;
            Rolled.AffixId = AffixId;
            Rolled.Tier = 6;
            Rolled.Category = Definition->Category;
            Rolled.Value = UBreakerAffixLibrary::ValueForTier(*Definition, 6);
            Item.Affixes.Add(Rolled);
        }
        return Item;
    };

    const TArray<FBreakerItemInstance> Bare;
    const FBreakerItemInstance WeaponPiece = MakeItem(EBreakerEquipSlot::Helmet, TEXT("Offense.WeaponDamage"));
    const TArray<FBreakerStatLine> Weapon = BreakerSkillProjection::ProjectEquip(Snapshot, Bare, WeaponPiece);
    if (!TestEqual(TEXT("The delta reports five rows"), Weapon.Num(), 5)) return false;

    // THE PROPERTY A SCALAR COULD NOT HAVE. A weapon-lane roll moves the
    // weapon lane and leaves the ability lane exactly where it was.
    TestTrue(*FString::Printf(TEXT("A Weapon Damage roll raises the weapon lane (%.4f -> %.4f)"),
        Weapon[0].Before, Weapon[0].After), Weapon[0].After > Weapon[0].Before);
    TestEqual(TEXT("...and does not move the ability lane at all"),
        Weapon[1].After, Weapon[1].Before, 0.0001f);

    const FBreakerItemInstance AbilityPiece = MakeItem(EBreakerEquipSlot::Helmet, TEXT("Offense.AbilityDamage"));
    const TArray<FBreakerStatLine> Ability = BreakerSkillProjection::ProjectEquip(Snapshot, Bare, AbilityPiece);
    TestTrue(*FString::Printf(TEXT("An Ability Damage roll raises the ability lane (%.4f -> %.4f)"),
        Ability[1].Before, Ability[1].After), Ability[1].After > Ability[1].Before);
    TestEqual(TEXT("...and does not move the weapon lane at all"),
        Ability[0].After, Ability[0].Before, 0.0001f);

    // EQUIPPING REPLACES. An item is worth what it displaces, so swapping a
    // good piece for a bare one of the same slot must read as a LOSS. A score
    // that summed the candidate alone would call this an upgrade.
    TArray<FBreakerItemInstance> Equipped;
    Equipped.Add(WeaponPiece);
    FBreakerItemInstance Downgrade;
    Downgrade.ItemId = FGuid::NewGuid();
    Downgrade.Slot = EBreakerEquipSlot::Helmet;
    Downgrade.ItemLevel = 50;
    const TArray<FBreakerStatLine> Worse = BreakerSkillProjection::ProjectEquip(Snapshot, Equipped, Downgrade);
    TestTrue(*FString::Printf(TEXT("Swapping to an affixless piece reads as a loss (%.4f -> %.4f)"),
        Worse[0].Before, Worse[0].After), Worse[0].After < Worse[0].Before);

    // EFFECTIVE HEALTH IS NAMED FOR WHAT IT EXCLUDES, and it must be at least
    // the health pool: dividing by survival can only raise it.
    const FBreakerItemInstance HealthPiece = MakeItem(EBreakerEquipSlot::BodyArmour, TEXT("Core.PhysicalDR"));
    const TArray<FBreakerStatLine> Defence = BreakerSkillProjection::ProjectEquip(Snapshot, Bare, HealthPiece);
    TestTrue(TEXT("The defence row names the damage type it covers"),
        Defence[4].Label.Contains(TEXT("PHYSICAL")));
    TestTrue(*FString::Printf(TEXT("Physical reduction raises effective health (%.1f -> %.1f)"),
        Defence[4].Before, Defence[4].After), Defence[4].After > Defence[4].Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRankedProjectionFloorTest,
    "RiorsEdge.UI.SkillProjection.NodeWithoutDamageProjectsNoDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRankedProjectionFloorTest::RunTest(const FString& Parameters)
{
    UBreakerProgressionNode* Minor = NewObject<UBreakerProgressionNode>();
    Minor->NodeId = TEXT("Projection.RankedMinor");
    Minor->MaxRank = 3;
    Minor->CostPerRank = 1;
    FBreakerSkillSnapshot Snapshot;
    Snapshot.Nodes.Add(Minor);
    Snapshot.IncreasedDamagePerSpentPoint = GetDefault<UBreakerProgressionComponent>()->IncreasedDamagePerSpentPoint;
    Snapshot.bHasComposedAttributes = true;
    Snapshot.Aggregator.SetBase(EBreakerAggregatedAttribute::DamageMultiplier, 1.0f);
    Snapshot.Aggregator.SetBase(EBreakerAggregatedAttribute::AbilityDamageMultiplier, 1.0f);
    FBreakerAttributeContribution Gear;
    Gear.AddSharedIncreasedDamage(300.0f);
    Snapshot.Aggregator.SetContribution(EBreakerAttributeContributor::Equipment, Gear);
    // O27: a node with no damage effect projects no damage on either lane, at
    // any rank, against composed gear — the panel shows a gain only where the
    // node is the gain.
    TestEqual(TEXT("The spend dial ships at zero"), Snapshot.IncreasedDamagePerSpentPoint, 0.0f);
    for (int32 Rank = 0; Rank < 3; ++Rank)
    {
        const TArray<FBreakerStatLine> Lines = BreakerSkillProjection::ProjectPurchase(Snapshot, Minor->NodeId, 1);
        for (int32 Lane = 0; Lane < 2; ++Lane)
        {
            TestFalse(TEXT("Projected against composed gear, not tree-only"), Lines[Lane].bTreeOnly);
            TestFalse(TEXT("A damage-less node projects no damage on this lane"), Lines[Lane].Changed());
        }
        Snapshot.Ranks = BreakerSkillProjection::WithRankDelta(Snapshot.Ranks, Minor->NodeId, 1);
    }
    for (const FBreakerStatLine& Line : BreakerSkillProjection::ProjectPurchase(Snapshot, Minor->NodeId, 1))
        TestFalse(TEXT("Max rank cannot project phantom point-spend power"), Line.Changed());
    for (const FBreakerStatLine& Line : BreakerSkillProjection::ProjectPurchase(Snapshot, TEXT("Unknown"), 1))
        TestFalse(TEXT("Unknown nodes cannot project phantom power"), Line.Changed());
    return true;
}

#endif
