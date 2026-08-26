#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/DefaultPawn.h"
#include "Game/BreakerRiftDefinition.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerRiftRewardMath.h"

// ---------------------------------------------------------------------------
// O168's third commit, proven: the rift completion payout. The pure math is
// pinned where it lives, and the handler is exercised by direct call — the
// same seam the BeginPlay bind routes into, minus the world the bind needs.
//
// WHAT THIS DOES NOT COVER: the bind itself (one authority-gated
// AddWeakLambda in BeginPlay — a live rift run is the check) and the
// first-clear grants, which wait on an archetype existing on the rift
// definition (O168 records why).
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftRewardMathTest,
    "RiorsEdge.Progression.RiftReward.Math",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftRewardMathTest::RunTest(const FString& Parameters)
{
    using namespace BreakerRiftReward;

    // Area level 1 pays exactly the authored bases — the same anchoring rule
    // the chassis holds (area level 1 is bit-identical to the seed).
    TestEqual(TEXT("AL1 Riftglass is the base"), RiftglassForCompletion(1), CompletionRiftglassBase);
    TestEqual(TEXT("AL1 XP is the base"), XpForCompletion(1), CompletionXpBase);

    // The payout rides the chassis's own growth constant — one number, one
    // place. Asserted against the authored default, not a restated 0.09.
    const float Growth = FBreakerMonsterChassisParams{}.HealthGrowthPerLevel;
    TestEqual(TEXT("the completion scale is the chassis's growth"),
        CompletionScale(2), 1.0f + Growth, 0.0001f);

    // Monotone and clamped: a deeper rift never pays less, and garbage area
    // levels clamp instead of exploding.
    int32 Previous = RiftglassForCompletion(1);
    for (int32 Level = 2; Level <= 100; ++Level)
    {
        const int32 Pay = RiftglassForCompletion(Level);
        TestTrue(*FString::Printf(TEXT("AL%d never pays less than AL%d"), Level, Level - 1), Pay >= Previous);
        Previous = Pay;
    }
    TestEqual(TEXT("an unset area level pays the clamp floor"),
        RiftglassForCompletion(0), RiftglassForCompletion(1));
    TestEqual(TEXT("a runaway area level pays the clamp ceiling"),
        RiftglassForCompletion(9999), RiftglassForCompletion(100));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftRewardPayoutTest,
    "RiorsEdge.Progression.RiftReward.Payout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftRewardPayoutTest::RunTest(const FString& Parameters)
{
    // A pawn owner, because the handler compares the event's pawn against its
    // owner — the guard under test.
    APawn* Owner = NewObject<ADefaultPawn>();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);

    FBreakerRiftDefinition Rift;
    Rift.AreaName = FText::FromString(TEXT("Test Substation"));
    Rift.AreaLevel = 3;

    const int32 ExpectedXp = BreakerRiftReward::XpForCompletion(3);
    const int32 ExpectedRiftglass = BreakerRiftReward::RiftglassForCompletion(3);
    const int32 XpBefore = Progression->GetProgressionState().TotalExperience;
    const int32 GlassBefore = Equipment->GetForgeWallet().Get();

    // A broadcast for SOMEBODY ELSE'S pawn pays this character nothing.
    APawn* Stranger = NewObject<ADefaultPawn>();
    Progression->HandleRiftCompleted(Rift, Stranger);
    TestEqual(TEXT("another pawn's completion pays no XP here"),
        Progression->GetProgressionState().TotalExperience, XpBefore);
    TestEqual(TEXT("another pawn's completion pays no Riftglass here"),
        Equipment->GetForgeWallet().Get(), GlassBefore);

    // The owner's completion pays both halves, at the math's own numbers.
    Progression->HandleRiftCompleted(Rift, Owner);
    TestEqual(TEXT("completion pays the composed XP"),
        Progression->GetProgressionState().TotalExperience, XpBefore + ExpectedXp);
    TestEqual(TEXT("completion pays the composed Riftglass onto the wallet"),
        Equipment->GetForgeWallet().Get(), GlassBefore + ExpectedRiftglass);

    // A second completion pays again — GROUND's latch makes a second
    // broadcast a genuinely new run, so the payout must not deduplicate what
    // the seam already guarantees is distinct.
    Progression->HandleRiftCompleted(Rift, Owner);
    TestEqual(TEXT("a new run's completion pays again"),
        Progression->GetProgressionState().TotalExperience, XpBefore + 2 * ExpectedXp);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
