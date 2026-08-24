#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/BreakerStatusEmit.h"
#include "Progression/BreakerBuildConditions.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTypes.h"

// ---------------------------------------------------------------------------
// A CONDITION THAT CAN NEVER BE TRUE IS WORSE THAN ONE NOTHING AUTHORS.
//
// `dead-conditions` already counts conditions no content authors. This counts
// the opposite and more dangerous thing: conditions that are authorABLE, appear
// in the enum, autocomplete, read as ordinary vocabulary — and are wired to a
// recorder that does not exist, so they return false on every frame forever.
//
// Four of them, the whole Recently* family: RecentlyKilled, RecentlyTookDamage,
// RecentlyCastAbility, RecentlyAppliedStatus. IsSelfEvaluable returns false for
// each unconditionally (BreakerBuildConditions.cpp:135). RecentlyDashed is the
// working template they were all written against and the only one with a
// recorder behind it.
//
// This was found by accident, while picking a condition for
// Swift.Frenzy.AmmunitionEconomy: "ammunition returned on a kill" makes
// RecentlyKilled the obvious gate, and a line authored there would have paid
// nothing for the rest of the project's life while LOOKING like the fix for an
// unconditional line. That is the same lying-instrument shape as a test that
// grants itself a state the game cannot produce — and it is worse, because a
// dead condition at least reads as dead.
//
// So it is a number now, not a sentence. Two assertions:
//
//   THE COUNT, emitted so it is tracked and falls when a recorder lands. It is
//   pinned as a CEILING: it may go down and must never go up.
//
//   NO CONTENT MAY AUTHOR ONE. This is the assertion that matters, and it is
//   the one that would have caught the mistake above at the commit that made it.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerConditionsThatCanNeverBeTrueTest,
    "RiorsEdge.Progression.ConditionVocabulary.Unevaluable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerConditionsThatCanNeverBeTrueTest::RunTest(const FString& Parameters)
{
    // ---- THE COUNT --------------------------------------------------------
    // Self conditions only. A target condition also answers false to
    // IsSelfEvaluable, but that is a statement about ONE-actor evaluation, not
    // about whether it can ever be true — target state supplies them, and
    // counting them here would drown the four that are actually broken.
    TArray<FString> Unevaluable;
    for (int32 Index = 0; Index < FBreakerBuildConditionState::ConditionCount; ++Index)
    {
        const EBreakerBuildCondition Condition = static_cast<EBreakerBuildCondition>(Index);
        if (Condition == EBreakerBuildCondition::Always) continue;
        if (FBreakerBuildConditionState::IsTargetCondition(Condition)) continue;
        if (FBreakerBuildConditionState::IsSelfEvaluable(Condition)) continue;
        Unevaluable.Add(FString(FBreakerBuildConditionState::DescribeCondition(Condition)));
    }

    AddInfo(FString::Printf(TEXT("Conditions that can never be true (%d): %s"),
        Unevaluable.Num(),
        Unevaluable.Num() ? *FString::Join(Unevaluable, TEXT(", ")) : TEXT("none")));
    BreakerStatus::Emit(TEXT("unevaluable-conditions"), static_cast<float>(Unevaluable.Num()));

    // Guard the guard: the enum must have been walked. A zero here that comes
    // from an empty loop is the same nothing this test exists to prevent.
    TestTrue(TEXT("The condition vocabulary was walked"),
        FBreakerBuildConditionState::ConditionCount > 1);

    // ---- AND NOTHING MAY AUTHOR ONE ---------------------------------------
    // Every effect on every node in every tree, including Core. An unevaluable
    // condition on an authored line is silent content that looks conditional.
    TArray<FString> Offenders;
    int32 EffectsWalked = 0;
    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
    {
        if (!Tree) continue;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (!Node) continue;
            for (const FBreakerNodeEffect& Effect : Node->Effects)
            {
                ++EffectsWalked;
                auto Check = [&](EBreakerBuildCondition Condition)
                {
                    if (Condition == EBreakerBuildCondition::Always) return;
                    if (FBreakerBuildConditionState::IsTargetCondition(Condition)) return;
                    if (FBreakerBuildConditionState::IsSelfEvaluable(Condition)) return;
                    Offenders.Add(FString::Printf(TEXT("%s on %s"),
                        *FString(FBreakerBuildConditionState::DescribeCondition(Condition)), *Node->NodeId.ToString()));
                };
                Check(Effect.Condition);
                for (const EBreakerBuildCondition Also : Effect.AlsoRequires) Check(Also);
            }
        }
    }

    TestTrue(TEXT("There are authored effects to check"), EffectsWalked > 50);
    TestEqual(*FString::Printf(
        TEXT("No authored effect is gated on a condition that can never be true: %s"),
        Offenders.Num() ? *FString::Join(Offenders, TEXT("; ")) : TEXT("none")),
        Offenders.Num(), 0);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
