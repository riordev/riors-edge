#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
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

    FRig MakeRig(int32 ClassPoints = 10, int32 CorePoints = 12)
    {
        FRig Rig;
        Rig.Owner = NewObject<AActor>();
        Rig.Attributes = NewObject<UBreakerAttributeSet>();
        Rig.Progression = NewObject<UBreakerProgressionComponent>(Rig.Owner);
        Rig.Progression->BindAttributes(Rig.Attributes);
        Rig.Progression->ChoosePermanentClassById(EBreakerClassId::Swift);
        Rig.Progression->GrantPlaytestPoints(ClassPoints, CorePoints);
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
    Rig.Progression->PurchaseNode(CoreTree(), TEXT("Core.Precision.Sightline"), Reason);
    Rig.Progression->PurchaseNode(CoreTree(), TEXT("Core.Bulwark.SetStance"), Reason);
    Rig.Progression->PurchaseNode(CoreTree(), TEXT("Core.Kinesis.LightFooting"), Reason);

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
    // Cyclic is the slice's clearest damage node: 3 ranks at +3% Increased
    // Damage each, 1 point per rank. It needs Trigger Discipline as a
    // prerequisite AND 2 points already invested in the tree (its Tier-2
    // gate), so two entry nodes come first.
    Rig.Progression->PurchaseNode(CoreTree(), TEXT("Core.Volley.TriggerDiscipline"), Reason);
    Rig.Progression->PurchaseNode(CoreTree(), TEXT("Core.Precision.Sightline"), Reason);

    const FBreakerSkillSnapshot Snapshot = BreakerSkillProjection::MakeSnapshot(Rig.Progression, Rig.Attributes);
    const TArray<FBreakerStatLine> OneRank = BreakerSkillProjection::ProjectPurchase(Snapshot, TEXT("Core.Volley.Cyclic"), 1);
    const FBreakerStatLine& Damage = OneRank[0];

    TestEqual(TEXT("The projection starts from the live damage number"),
        Damage.Before, Rig.Attributes->GetDamageMultiplier(), 0.0001f);
    TestTrue(TEXT("Buying a damage node is projected to move damage"), Damage.Changed());
    // +3% from the node effect and +0.25% from the per-spent-point baseline,
    // both in the one additive Increased bucket. The baseline was 1% until O27
    // cut it to a floor; the node's own effect is what carries a purchase now,
    // which is the point of the ruling.
    TestEqual(TEXT("One rank of Cyclic projects +3.25% damage"), Damage.After - Damage.Before, 0.0325f, 0.0005f);

    // Now actually buy it. The arrow has to have been telling the truth.
    TestTrue(TEXT("Cyclic is purchasable"), Rig.Progression->PurchaseNode(CoreTree(), TEXT("Core.Volley.Cyclic"), Reason));
    TestEqual(TEXT("The real purchase lands exactly where the projection pointed"),
        Rig.Attributes->GetDamageMultiplier(), Damage.After, 0.0001f);

    // Buying to max is the same arithmetic three ranks deep, and the screen
    // offers it on SHIFT+LMB, so it is projected the same way.
    const FBreakerSkillSnapshot AfterOne = BreakerSkillProjection::MakeSnapshot(Rig.Progression, Rig.Attributes);
    const TArray<FBreakerStatLine> ToMax = BreakerSkillProjection::ProjectPurchase(AfterOne, TEXT("Core.Volley.Cyclic"), 2);
    // 2 x (3% node + 0.25% baseline).
    TestEqual(TEXT("Two more ranks project +6.5% damage"), ToMax[0].After - ToMax[0].Before, 0.065f, 0.0005f);
    TestTrue(TEXT("Cyclic rank 2"), Rig.Progression->PurchaseNode(CoreTree(), TEXT("Core.Volley.Cyclic"), Reason));
    TestTrue(TEXT("Cyclic rank 3"), Rig.Progression->PurchaseNode(CoreTree(), TEXT("Core.Volley.Cyclic"), Reason));
    TestEqual(TEXT("Buying to max lands where the projection pointed"),
        Rig.Attributes->GetDamageMultiplier(), ToMax[0].After, 0.0001f);

    // A node with no effects still costs a point, and the point itself pays.
    // That is the whole reason the baseline exists, so the screen must show it.
    const FBreakerSkillSnapshot AfterMax = BreakerSkillProjection::MakeSnapshot(Rig.Progression, Rig.Attributes);
    const TArray<FBreakerStatLine> Inert = BreakerSkillProjection::ProjectPurchase(AfterMax, TEXT("Core.Affliction.OpenWound"), 1);
    TestEqual(TEXT("An effectless node still projects its point-spend baseline"),
        Inert[0].After - Inert[0].Before, 0.0025f, 0.0005f);
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
    Committed.Add({TEXT("Core.Volley.Cyclic"), 3});             // cost 1 x 3
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
        BreakerSkillProjection::ProjectPurchase(Fresh, TEXT("Core.Volley.Cyclic"), 1)[0].Before, 1.0f, 0.0001f);
    return true;
}

#endif
