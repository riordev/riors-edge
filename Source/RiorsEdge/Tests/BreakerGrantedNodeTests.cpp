#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionTypes.h"

// ---------------------------------------------------------------------------
// ORDERS ruling 1's granted node: Swift's enhanced-dash passive (Longstride),
// seeded at rank 1 wherever a character becomes or loads as Swift. The three
// ruled properties are each pinned: the grant exists on every path a Swift
// can arrive through, a respec never takes it (and never refunds it — cost 0
// makes the arithmetic the guarantee), and the lane actually pays.
//
// WHAT THIS DOES NOT COVER: the dash impulse itself. TryDash refuses on a
// bare component (no world), so the movement-side multiply is one line read
// per press, asserted here only through the composed lane; whether +20% dash
// READS as further is the owner's playtest, not a number.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSwiftGrantedDashNodeTest,
    "RiorsEdge.Progression.GrantedNodes.SwiftLongstride",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSwiftGrantedDashNodeTest::RunTest(const FString& Parameters)
{
    const FName NodeId = UBreakerProgressionComponent::SwiftGrantedDashNodeId;

    // The node exists where the seed points, costs nothing, and carries the
    // DashDistance line — a dangling seed id would be a rank of nothing.
    const UBreakerProgressionNode* Node = UBreakerProgressionLibrary::FindFallbackNode(NodeId);
    if (!TestNotNull(TEXT("the granted node exists in a tree"), Node)) return false;
    TestEqual(TEXT("the granted node costs nothing (respec-no-refund as arithmetic)"), Node->CostPerRank, 0);
    TestEqual(TEXT("the granted node is single-rank today"), Node->MaxRank, 1);

    // Choice path: locking Swift seeds it, pays it, and spends nothing.
    {
        AActor* Owner = NewObject<AActor>();
        UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
        TestTrue(TEXT("Swift locks"), Progression->ChoosePermanentClassById(EBreakerClassId::Swift));
        TestEqual(TEXT("locking Swift seeds the granted rank"),
            Progression->GetNodeRank(NodeId, EBreakerPointCurrency::DoctrinePoints), 1);
        TestEqual(TEXT("the granted rank spends nothing"), Progression->GetSpentPoints(), 0.0f, 0.0001f);
        TestEqual(TEXT("the dash lane pays from the grant alone"),
            Progression->GetNodeStats().DashDistanceMultiplier, 1.20f, 0.0001f);

        // The respec property, on the same character: a doctrine respec
        // clears every doctrine rank and refunds what was paid — the granted
        // rank was never paid for, so it survives and refunds nothing.
        const int32 DoctrineBefore = Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints);
        FText Failure;
        TestTrue(TEXT("a Forge doctrine respec succeeds"),
            Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, Failure));
        TestEqual(TEXT("the granted rank survives the respec"),
            Progression->GetNodeRank(NodeId, EBreakerPointCurrency::DoctrinePoints), 1);
        TestEqual(TEXT("the respec refunded nothing for it"),
            Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), DoctrineBefore);
        TestEqual(TEXT("the lane still pays after the respec"),
            Progression->GetNodeStats().DashDistanceMultiplier, 1.20f, 0.0001f);
    }

    // Load path: a Swift save written with no doctrine ranks — a migration,
    // or the roster's direct write — arrives holding the grant.
    {
        AActor* Owner = NewObject<AActor>();
        UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
        FBreakerProgressionState Saved;
        Saved.PermanentClass = EBreakerClassId::Swift;
        Progression->LoadProgressionState(Saved);
        TestEqual(TEXT("a rankless Swift save is seeded on load"),
            Progression->GetNodeRank(NodeId, EBreakerPointCurrency::DoctrinePoints), 1);
    }

    // The grant is Swift's, not everyone's.
    {
        AActor* Owner = NewObject<AActor>();
        UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
        TestTrue(TEXT("Caster locks"), Progression->ChoosePermanentClassById(EBreakerClassId::Caster));
        TestEqual(TEXT("a Caster holds no Swift grant"),
            Progression->GetNodeRank(NodeId, EBreakerPointCurrency::DoctrinePoints), 0);
        TestEqual(TEXT("a Caster's dash lane is identity"),
            Progression->GetNodeStats().DashDistanceMultiplier, 1.0f, 0.0001f);
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
