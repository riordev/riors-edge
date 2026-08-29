#include "Misc/AutomationTest.h"
#include "Combat/BreakerEnemyBodyMath.h"
#include "Combat/BreakerEnemy.h"
#include "UObject/UObjectIterator.h"

#if WITH_DEV_AUTOMATION_TESTS

// The named-body fit is pure maths (BreakerEnemyBodyMath.h), so the rule is
// proved here without a world: the mesh fills the capsule's height, the
// bounds offset cancels at the fitted scale, and a degenerate mesh degrades
// to identity instead of dividing into an infinity. The second test asserts
// the SHIPPED CONFIGURATION: no enemy ships a named body — the hook exists
// for the intake meshes and the preview command, and defaulting one on is
// FIELD's readability call. The day a chassis legitimately sets one, this
// pin moves in the same commit, deliberately.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEnemyBodyFitTest,
    "RiorsEdge.Combat.EnemyBody.FitFillsCapsuleAndCancelsOffset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEnemyBodyFitTest::RunTest(const FString& Parameters)
{
    using BreakerEnemyBody::FitBodyToCapsule;

    // A mesh authored 2 m tall (half-height 100) on a 90 half-height capsule
    // scales to fill it exactly.
    {
        const auto Fit = FitBodyToCapsule(FVector::ZeroVector, FVector(40.0, 30.0, 100.0), 90.0f);
        TestEqual(TEXT("scale fills the capsule"), Fit.Scale, 0.9f, 1e-4f);
        TestEqual(TEXT("centred mesh stays centred"), Fit.RelativeLocation, FVector::ZeroVector);
    }
    // A baked scene offset cancels AT THE FITTED SCALE — the NPC route's
    // hill-sized-mesh defect, stated as arithmetic.
    {
        const FVector Origin(500.0, 300.0, 80.0);
        const auto Fit = FitBodyToCapsule(Origin, FVector(40.0, 30.0, 180.0), 90.0f);
        TestEqual(TEXT("scale"), Fit.Scale, 0.5f, 1e-4f);
        TestEqual(TEXT("offset cancels scaled"), Fit.RelativeLocation, FVector(-250.0, -150.0, -40.0), 1e-3);
    }
    // Degenerate bounds refuse the fit rather than exploding.
    {
        const auto Fit = FitBodyToCapsule(FVector::ZeroVector, FVector::ZeroVector, 90.0f);
        TestEqual(TEXT("degenerate mesh keeps identity scale"), Fit.Scale, 1.0f, 1e-6f);
        TestEqual(TEXT("degenerate mesh keeps identity location"), Fit.RelativeLocation, FVector::ZeroVector);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEnemyBodyDefaultsOffTest,
    "RiorsEdge.Combat.EnemyBody.NoEnemyShipsANamedBody",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEnemyBodyDefaultsOffTest::RunTest(const FString& Parameters)
{
    // Every registered enemy class, not just the base: a subclass constructor
    // that quietly sets a body would change the shipped look of its whole
    // family while this test stayed green on the base CDO alone.
    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (!It->IsChildOf(ABreakerEnemy::StaticClass())) continue;
        if (It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;
        const ABreakerEnemy* Defaults = It->GetDefaultObject<ABreakerEnemy>();
        if (!Defaults) continue;
        TestFalse(FString::Printf(TEXT("%s ships no named body"), *It->GetName()),
                  Defaults->BodyMeshAsset.IsValid());
    }
    return true;
}

#endif
