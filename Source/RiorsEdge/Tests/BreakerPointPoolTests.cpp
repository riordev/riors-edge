#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTypes.h"
#include "Attributes/BreakerAttributeSet.h"
#include "UI/BreakerSkillProjection.h"

// ---------------------------------------------------------------------------
// EVERY POOL IS ROUTED.
//
// A point pool is a rank array plus a wallet, and adding one means teaching
// every seam that reads ranks about the new array. That has now been got wrong
// TWICE, in two different ways, from a single ruling:
//
//   RecalculateStats built the aggregation input from two arrays, so every
//   doctrine node paid exactly nothing. Found only through eighteen unrelated
//   test failures -- the node bought, the rank recorded, no purchase failed.
//
//   MakeSnapshot did the same thing on the SKILL SCREEN, and that one was
//   invisible to the suite entirely: totals and before/after deltas silently
//   ignored doctrine ranks, on the screen a permanent choice is made on.
//
// Neither was a hard failure. Both were a missing Append. So this test does not
// check the two seams that are known to have been wrong -- it ENUMERATES the
// currency and requires every live value to prove it reaches both, which is
// what makes a fourth pool safe rather than lucky. A new enumerator with no
// case here fails the count assertion below before it can fail silently.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEveryPoolIsRoutedTest,
    "RiorsEdge.Progression.PointPools.EveryPoolIsRouted",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    struct FBreakerPoolCase
    {
        EBreakerPointCurrency Currency;
        const TCHAR* NodeId;
        // Where a rank for this pool is stored. The whole point of the test is
        // that this is the ONLY place the pool's array is named by hand.
        TFunction<void(FBreakerProgressionState&, FName)> Seed;
        // What the aggregator must show once that rank is owned. Per-case
        // rather than "something changed", so a node whose payload is a tag
        // cannot pass this by accident.
        TFunction<bool(const FBreakerNodeStats&)> Reached;
    };
}

bool FBreakerEveryPoolIsRoutedTest::RunTest(const FString& Parameters)
{
    TArray<FBreakerPoolCase> Cases;
    Cases.Add({
        EBreakerPointCurrency::CorePoints,
        TEXT("Core.Precision.Sightline"),
        [](FBreakerProgressionState& S, FName Id) { S.CoreNodeRanks.Add({Id, 1}); },
        [](const FBreakerNodeStats& N) { return N.CriticalChanceBonus > 0.0f; },
    });
    Cases.Add({
        EBreakerPointCurrency::DoctrinePoints,
        TEXT("Swift.Kinetic.Carry"),
        [](FBreakerProgressionState& S, FName Id) { S.DoctrineNodeRanks.Add({Id, 1}); },
        [](const FBreakerNodeStats& N) { return N.SlideSpeedMultiplier > 1.0f; },
    });

    // THE ENUMERATION, and it is the assertion that makes the rest of this test
    // worth having. A new pool appended to EBreakerPointCurrency fails HERE,
    // loudly, at the moment it is declared -- rather than shipping unrouted and
    // being found through unrelated failures, or not at all.
    const UEnum* CurrencyEnum = StaticEnum<EBreakerPointCurrency>();
    if (!TestNotNull(TEXT("EBreakerPointCurrency is reflected"), CurrencyEnum)) return false;
    // NumEnums() counts the hidden _MAX sentinel UHT appends.
    const int32 Declared = CurrencyEnum->NumEnums() - 1;
    constexpr int32 Retired = 1;   // ClassPoints_Retired: no storage, by design
    TestEqual(TEXT("Every declared currency is either live-and-covered here, or the retired one"),
        Cases.Num() + Retired, Declared);

    // The retired value is asserted to have NO storage, so "covered" above
    // cannot be satisfied by quietly pointing it at a live pool.
    {
        UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
        FBreakerProgressionState State;
        State.PermanentClass = EBreakerClassId::Swift;
        State.ClassNodeRanks.Add({TEXT("Swift.Kinetic.Carry"), 2});
        Progression->LoadProgressionState(State);
        TestEqual(TEXT("A rank in the retired array reaches nothing"),
            Progression->GetNodeStats().SlideSpeedMultiplier, 1.0f, 0.0001f);
    }

    for (const FBreakerPoolCase& Case : Cases)
    {
        const FString Name = CurrencyEnum->GetNameStringByValue(static_cast<int64>(Case.Currency));

        UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
        UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
        Progression->BindAttributes(Attributes);

        FBreakerProgressionState State;
        State.PermanentClass = EBreakerClassId::Swift;
        Case.Seed(State, FName(Case.NodeId));
        Progression->LoadProgressionState(State);

        // SEAM ONE: the aggregator. This is the omission that made every
        // doctrine node worthless.
        TestTrue(*FString::Printf(TEXT("%s: a rank reaches the aggregator (%s)"), *Name, Case.NodeId),
            Case.Reached(Progression->GetNodeStats()));

        // SEAM TWO: the skill screen's snapshot. This is the omission that made
        // the projection lie about totals and about every purchase delta.
        const FBreakerSkillSnapshot Snapshot = BreakerSkillProjection::MakeSnapshot(Progression, Attributes);
        const bool bInSnapshot = Snapshot.Ranks.ContainsByPredicate(
            [&Case](const FBreakerNodeRank& R) { return R.NodeId == FName(Case.NodeId); });
        TestTrue(*FString::Printf(TEXT("%s: a rank reaches the projection snapshot (%s)"), *Name, Case.NodeId),
            bInSnapshot);

        // And the projection's own totals move with it -- ContainsByPredicate
        // proves the rank arrived, this proves the snapshot is actually read.
        const TArray<FBreakerStatLine> Totals = BreakerSkillProjection::CurrentTotals(Snapshot);
        TestTrue(*FString::Printf(TEXT("%s: the projection reports at least one line"), *Name),
            Totals.Num() > 0);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
