#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/DefaultPawn.h"
#include "Game/BreakerRiftDefinition.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerRiftRewardMath.h"
#include "Save/BreakerAccountSave.h"

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
    // A transient, never-persisting account, injected so the suite exercises
    // the record and the first-clear rule WITHOUT touching the machine's
    // real account slot.
    UBreakerAccountSave* Account = NewObject<UBreakerAccountSave>();
    Account->bNeverPersist = true;
    UBreakerAccountSave::InjectForTesting(Account);

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

    // A broadcast for SOMEBODY ELSE'S pawn pays this character nothing —
    // and advances no record.
    APawn* Stranger = NewObject<ADefaultPawn>();
    Progression->HandleRiftCompleted(Rift, Stranger);
    TestEqual(TEXT("another pawn's completion pays no XP here"),
        Progression->GetProgressionState().TotalExperience, XpBefore);
    TestEqual(TEXT("another pawn's completion advances no record"), Account->HighestClearedAreaLevel, 0);

    // The FIRST clear pays both halves and advances the account record
    // (One-AA: a first clear pays the ladder).
    Progression->HandleRiftCompleted(Rift, Owner);
    TestEqual(TEXT("first clear pays the composed XP"),
        Progression->GetProgressionState().TotalExperience, XpBefore + ExpectedXp);
    TestEqual(TEXT("first clear pays the composed Riftglass onto the wallet"),
        Equipment->GetForgeWallet().Get(), GlassBefore + ExpectedRiftglass);
    TestEqual(TEXT("first clear advances the account record"), Account->HighestClearedAreaLevel, 3);

    // A RE-CLEAR pays no purse and moves no record — going back is allowed;
    // going back is not the game. (Drops and kill XP never route through
    // this handler and are untouched.)
    Progression->HandleRiftCompleted(Rift, Owner);
    TestEqual(TEXT("a re-clear pays no purse"),
        Progression->GetProgressionState().TotalExperience, XpBefore + ExpectedXp);
    TestEqual(TEXT("a re-clear moves no record"), Account->HighestClearedAreaLevel, 3);

    // A DEEPER first clear pays again and advances again — the ladder is
    // climbed once per rung, not once.
    FBreakerRiftDefinition Deeper = Rift;
    Deeper.AreaLevel = 5;
    Progression->HandleRiftCompleted(Deeper, Owner);
    TestEqual(TEXT("a deeper first clear pays its own purse"),
        Progression->GetProgressionState().TotalExperience,
        XpBefore + ExpectedXp + BreakerRiftReward::XpForCompletion(5));
    TestEqual(TEXT("and advances the record to it"), Account->HighestClearedAreaLevel, 5);

    UBreakerAccountSave::ResetCacheForTesting();
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
