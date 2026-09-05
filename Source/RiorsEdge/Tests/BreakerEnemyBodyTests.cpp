#include "Misc/AutomationTest.h"
#include "Combat/BreakerBodyPaint.h"
#include "Combat/BreakerEnemyBodyMath.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Materials/MaterialInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/UObjectIterator.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

// The named-body fit is pure maths (BreakerEnemyBodyMath.h), so the rule is
// proved here without a world: the mesh fills the capsule's height, the
// bounds offset cancels at the fitted scale, and a degenerate mesh degrades
// to identity instead of dividing into an infinity. The second test asserts
// the SHIPPED CONFIGURATION — the mech cast — and its own comment below
// tells the story of the pin that moved.

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
        TestEqual(TEXT("offset cancels scaled"), Fit.RelativeLocation, FVector(-250.0, -150.0, -40.0), 1e-3f);
    }
    // Degenerate bounds refuse the fit rather than exploding.
    {
        const auto Fit = FitBodyToCapsule(FVector::ZeroVector, FVector::ZeroVector, 90.0f);
        TestEqual(TEXT("degenerate mesh keeps identity scale"), Fit.Scale, 1.0f, 1e-6f);
        TestEqual(TEXT("degenerate mesh keeps identity location"), Fit.RelativeLocation, FVector::ZeroVector);
        TestTrue(TEXT("degenerate mesh keeps identity rotation"),
            Fit.RelativeRotation.Equals(FRotator::ZeroRotator, 1e-6f));
    }
    // No forward axis given (a rig with no bilateral pair) keeps the yaw at
    // identity — the fit never invents an axis.
    {
        const auto Fit = FitBodyToCapsule(FVector::ZeroVector, FVector(40.0, 30.0, 100.0), 90.0f);
        TestTrue(TEXT("no axis, no yaw"), Fit.RelativeRotation.Equals(FRotator::ZeroRotator, 1e-6f));
    }
    // A mesh authored facing +Y (the Blender/glTF biped) is yawed -90 onto
    // +X, and the bounds origin cancels THROUGH that yaw: (250, 150, 40) at
    // the fitted scale turns to (150, -250, 40) before it is negated.
    {
        const FVector Origin(500.0, 300.0, 80.0);
        const auto Fit = FitBodyToCapsule(Origin, FVector(40.0, 30.0, 180.0), 90.0f, FVector(0.0, 1.0, 0.0));
        TestTrue(TEXT("+Y forward yaws -90"), Fit.RelativeRotation.Equals(FRotator(0.0, -90.0, 0.0), 1e-3f));
        TestTrue(TEXT("the yaw carries the mesh forward onto +X"),
            Fit.RelativeRotation.RotateVector(FVector(0.0, 1.0, 0.0)).Equals(FVector::ForwardVector, 1e-3));
        TestEqual(TEXT("offset cancels scaled through the yaw"), Fit.RelativeLocation, FVector(-150.0, 250.0, -40.0), 1e-2f);
    }
    // A mesh already facing +X is left exactly where it was.
    {
        const auto Fit = FitBodyToCapsule(FVector::ZeroVector, FVector(40.0, 30.0, 100.0), 90.0f, FVector::ForwardVector);
        TestTrue(TEXT("+X forward needs no yaw"), Fit.RelativeRotation.Equals(FRotator::ZeroRotator, 1e-6f));
    }
    return true;
}

// The forward-axis read is pure (BodyForwardAxisFromBilateralBones): a
// left/right pair spans the body's right, and Cross(Right, Up) is forward in
// Unreal's frame. Proved beside the fit so the sign convention is asserted
// once, where the arithmetic lives, and not re-derived from a photograph.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEnemyBodyForwardAxisTest,
    "RiorsEdge.Combat.EnemyBody.ForwardAxisReadsFromBilateralBones",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEnemyBodyForwardAxisTest::RunTest(const FString& Parameters)
{
    using BreakerEnemyBody::BodyForwardAxisFromBilateralBones;

    // Unreal's own frame: right side at +Y, so the body faces +X.
    {
        const FVector Axis = BodyForwardAxisFromBilateralBones(FVector(0.0, -20.0, 150.0), FVector(0.0, 20.0, 150.0));
        TestTrue(TEXT("right at +Y faces +X"), Axis.Equals(FVector::ForwardVector, 1e-6));
    }
    // The Blender/glTF biped: right side at -X, so the body faces +Y.
    {
        const FVector Axis = BodyForwardAxisFromBilateralBones(FVector(20.0, 0.0, 150.0), FVector(-20.0, 0.0, 150.0));
        TestTrue(TEXT("right at -X faces +Y"), Axis.Equals(FVector(0.0, 1.0, 0.0), 1e-6));
    }
    // Mirrored inputs flip the axis.
    {
        const FVector Axis = BodyForwardAxisFromBilateralBones(FVector(0.0, 20.0, 150.0), FVector(0.0, -20.0, 150.0));
        TestTrue(TEXT("swapped pair faces -X"), Axis.Equals(-FVector::ForwardVector, 1e-6));
    }
    // Height differences between the pair never tilt the answer.
    {
        const FVector Axis = BodyForwardAxisFromBilateralBones(FVector(0.0, -20.0, 190.0), FVector(0.0, 20.0, 110.0));
        TestTrue(TEXT("a slouched pair still reads flat"), Axis.Equals(FVector::ForwardVector, 1e-6));
    }
    // Coincident (or vertically stacked) inputs span nothing and return Zero.
    {
        TestTrue(TEXT("coincident pair returns Zero"),
            BodyForwardAxisFromBilateralBones(FVector(5.0, 5.0, 100.0), FVector(5.0, 5.0, 100.0)).IsZero());
        TestTrue(TEXT("stacked pair returns Zero"),
            BodyForwardAxisFromBilateralBones(FVector(5.0, 5.0, 100.0), FVector(5.0, 5.0, 160.0)).IsZero());
    }
    return true;
}

// This pin was NoEnemyShipsANamedBody for exactly one cycle — the hook landed
// default-off pending the readability call, and the owner then ruled the mech
// cast ON (2026-08-29). The pin moves with the ruling, per its own note:
// every enemy class either RESOLVES its shipped body (a renamed uasset fails
// here, not in a screenshot) or is the ranged Lattice, which must ship NONE —
// composed primitives by the same ruling. Gated on the imported mechs
// existing, the shipped-samples shape: a clean clone still fights as
// primitives and still passes.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEnemyBodyCastTest,
    "RiorsEdge.Combat.EnemyBody.MechCastResolvesAndLatticeStaysPrimitive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEnemyBodyCastTest::RunTest(const FString& Parameters)
{
    const FString MechDir = FPaths::ProjectContentDir() / TEXT("Breaker/Meshes/enemies/mechs");
    if (!IFileManager::Get().DirectoryExists(*MechDir))
    {
        return true;
    }
    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (!It->IsChildOf(ABreakerEnemy::StaticClass())) continue;
        if (It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;
        const ABreakerEnemy* Defaults = It->GetDefaultObject<ABreakerEnemy>();
        if (!Defaults) continue;
        const bool bIsLattice = It->IsChildOf(ABreakerRangedEnemy::StaticClass());
        if (bIsLattice)
        {
            TestFalse(FString::Printf(TEXT("%s (Lattice) ships primitives by ruling"), *It->GetName()),
                      Defaults->BodyMeshAsset.IsValid());
            continue;
        }
        TestTrue(FString::Printf(TEXT("%s ships a named body"), *It->GetName()),
                 Defaults->BodyMeshAsset.IsValid());
        if (Defaults->BodyMeshAsset.IsValid())
        {
            TestNotNull(*FString::Printf(TEXT("%s's body resolves: %s"), *It->GetName(),
                    *Defaults->BodyMeshAsset.ToString()),
                Defaults->BodyMeshAsset.TryLoad());
        }
        if (Defaults->BodyIdleAnimation.IsValid())
        {
            TestNotNull(*FString::Printf(TEXT("%s's gait resolves: %s"), *It->GetName(),
                    *Defaults->BodyIdleAnimation.ToString()),
                Defaults->BodyIdleAnimation.TryLoad());
        }
    }
    // THE PAINT PORT'S HALF OF THE CAST: the overlay material the reaction
    // layer wears over a named body must ship beside the mechs, or every
    // flash, badge, wash and burn silently vanishes from the whole cast —
    // the exact recorded cost this asset exists to close.
    TestNotNull(TEXT("the paint overlay material ships"),
        LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Breaker/Materials/M_BreakerBodyOverlay.M_BreakerBodyOverlay")));
    return true;
}

// The overlay STRENGTH is pure (BreakerBodyPaint::ResolveOverlayStrength), so
// its contract is proved without a mesh: the livery is PURE at rest, the
// reactions occlude exactly as they do on primitives, the rank badge wears
// the same authored blend weights as the primitive blend (one table, not
// two), and the wound wash rises with damage but never fully hides the paint
// job outside a reaction.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEnemyBodyOverlayStrengthTest,
    "RiorsEdge.Combat.EnemyBody.OverlayRestsPureAndOccludesInReaction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEnemyBodyOverlayStrengthTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBodyPaint;
    FState State;
    TestEqual(TEXT("rest is pure livery"), ResolveOverlayStrength(State), 0.0f, 1e-6f);

    State.Reaction = EReaction::Flash;
    TestEqual(TEXT("flash occludes"), ResolveOverlayStrength(State), 1.0f, 1e-6f);
    State.Reaction = EReaction::DeathCrumple;
    State.ReactionAlpha = 0.5f;
    TestEqual(TEXT("burn occludes for its whole ride"), ResolveOverlayStrength(State), 1.0f, 1e-6f);

    State = FState();
    State.Rank = EBreakerMonsterRank::Elite;
    TestEqual(TEXT("elite badge wears the primitive blend weight"),
        ResolveOverlayStrength(State), RankBlendFor(EBreakerMonsterRank::Elite), 1e-6f);
    State.Rank = EBreakerMonsterRank::ModifierBearing;
    TestEqual(TEXT("modifier badge likewise"),
        ResolveOverlayStrength(State), RankBlendFor(EBreakerMonsterRank::ModifierBearing), 1e-6f);

    State = FState();
    State.bHealthRamp = true;
    State.HealthFraction = 1.0f;
    TestEqual(TEXT("full health adds no wash"), ResolveOverlayStrength(State), 0.0f, 1e-6f);
    State.HealthFraction = 0.0f;
    const float Drained = ResolveOverlayStrength(State);
    TestTrue(TEXT("drained wash is visible but never occludes"), Drained > 0.3f && Drained < 1.0f);
    return true;
}

// A REVIVE RETURNS THE NAMED BODY TO THE FIT'S OWN TRANSFORM — all three
// channels. The owner photographed live mechs frozen sideways: the death
// one-shot's final frame, held forever, because the standing respawn path
// never re-played the gait (the pool's revive did — one door of three).
// Every revive routes through ApplyBodyMesh, and ApplyBodyMesh re-applies
// the fit's rotation alongside its scale and location so no future writer
// of the third channel can leak through a revive either. The contract is
// "the fit's own rotation", not identity: the fit yaws the mesh's rig-read
// forward onto +X, so identity is the wrong answer for a +Y-authored mech.
// NewObject, no world, the BreakerGameModeTests idiom — ReviveFromPool's
// transform half runs fine unregistered, and the unregistered part list
// paints nothing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEnemyBodyReviveResetTest,
    "RiorsEdge.Combat.EnemyBody.ReviveResetsNamedBodyTransform",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEnemyBodyReviveResetTest::RunTest(const FString& Parameters)
{
    // Gated exactly as the mech-cast pin is: a clean clone without the
    // imported packs fights as primitives and has no transform to pin.
    const FString MechDir = FPaths::ProjectContentDir() / TEXT("Breaker/Meshes/enemies/mechs");
    if (!IFileManager::Get().DirectoryExists(*MechDir))
    {
        return true;
    }

    ABreakerEnemy* Enemy = NewObject<ABreakerEnemy>();
    if (!Enemy)
    {
        AddError(TEXT("Could not construct an enemy to revive."));
        return false;
    }
    Enemy->ApplyBodyMesh();
    USkeletalMeshComponent* Body = Enemy->GetNamedBody();
    TestNotNull(TEXT("The enemy carries a named body component"), Body);
    if (!Body) return false;
    TestNotNull(TEXT("The shipped mesh resolved onto it"), Body->GetSkeletalMeshAsset());

    const FVector FitLocation = Body->GetRelativeLocation();
    const FVector FitScale = Body->GetRelativeScale3D();
    const FRotator FitRotation = Body->GetRelativeRotation();

    // What an accumulated root-motion death leaves behind: the component
    // rotated and slid off the fit. The dirtying yaw is chosen so it is not
    // the fit's own answer for any mesh facing a cardinal axis.
    Body->SetRelativeRotation(FitRotation + FRotator(10.0f, 37.0f, 45.0f));
    Body->AddRelativeLocation(FVector(120.0f, -40.0f, 15.0f));
    Body->SetRelativeScale3D(FitScale * 1.5f);

    Enemy->ReviveFromPool(FVector::ZeroVector);

    TestTrue(TEXT("A revived body stands at the fit's own rotation"),
        Body->GetRelativeRotation().Equals(FitRotation, 1.0e-4f));
    TestTrue(TEXT("A revived body sits at the fit's own location"),
        Body->GetRelativeLocation().Equals(FitLocation, 1.0e-3f));
    TestTrue(TEXT("A revived body wears the fit's own scale"),
        Body->GetRelativeScale3D().Equals(FitScale, 1.0e-4f));
    return true;
}

// THE NAMED BODY FACES WHERE THE ACTOR FACES. A mech authored facing +Y
// with an identity yaw looks to its actor's right while the actor itself is
// turned correctly — the gym reads "everyone looking left". The fit reads
// each rig's forward from a left/right bone pair and yaws it onto +X, so the
// body looks where the actor looks. Pinned on the SHIPPED cast, one
// class at a time: the rig offers a pair (Leela has no arms — her legs carry
// her), the fitted forward lands within 15 degrees of +X, and on a built
// enemy the body's world forward agrees with the actor's. Gated on the
// imported mechs existing, exactly as the cast pin is.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEnemyBodyFacingTest,
    "RiorsEdge.Combat.EnemyBody.NamedBodyFacesActorForward",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEnemyBodyFacingTest::RunTest(const FString& Parameters)
{
    const FString MechDir = FPaths::ProjectContentDir() / TEXT("Breaker/Meshes/enemies/mechs");
    if (!IFileManager::Get().DirectoryExists(*MechDir))
    {
        return true;
    }
    constexpr float FacingToleranceDeg = 15.0f; // O2 PLACEHOLDER — the same tolerance Breaker.Nav.Probe judges by
    const auto DegreesBetween2D = [](const FVector& A, const FVector& B) -> float
    {
        const FVector FlatA = A.GetSafeNormal2D();
        const FVector FlatB = B.GetSafeNormal2D();
        return FMath::RadiansToDegrees(static_cast<float>(
            FMath::Acos(FMath::Clamp(FVector::DotProduct(FlatA, FlatB), -1.0, 1.0))));
    };

    int32 MeshesChecked = 0;
    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (!It->IsChildOf(ABreakerEnemy::StaticClass())) continue;
        if (It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;
        if (It->IsChildOf(ABreakerRangedEnemy::StaticClass())) continue;
        const ABreakerEnemy* Defaults = It->GetDefaultObject<ABreakerEnemy>();
        if (!Defaults || !Defaults->BodyMeshAsset.IsValid()) continue;
        USkeletalMesh* Mesh = Cast<USkeletalMesh>(Defaults->BodyMeshAsset.TryLoad());
        if (!Mesh) continue; // the cast pin reports an unresolved body; this test is about facing
        ++MeshesChecked;
        const FString Name = It->GetName();

        // The pure half: the rig offers a pair and the fit yaws it onto +X.
        const FVector MeshForward = ABreakerEnemy::ReadBodyMeshForwardAxis(Mesh);
        TestFalse(FString::Printf(TEXT("%s's rig offers a left/right pair to read a forward from"), *Name),
            MeshForward.IsNearlyZero());
        if (MeshForward.IsNearlyZero()) continue;
        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const BreakerEnemyBody::FBreakerBodyFit Fit = BreakerEnemyBody::FitBodyToCapsule(
            Bounds.Origin, Bounds.BoxExtent, 90.0f, MeshForward);
        const float FittedOffDeg = DegreesBetween2D(Fit.RelativeRotation.RotateVector(MeshForward), FVector::ForwardVector);
        TestTrue(FString::Printf(TEXT("%s's fitted forward is within %.0f deg of +X (was %.1f)"), *Name, FacingToleranceDeg, FittedOffDeg),
            FittedOffDeg <= FacingToleranceDeg);

        // The wired half: a built enemy's body forward agrees with its actor.
        ABreakerEnemy* Enemy = NewObject<ABreakerEnemy>(GetTransientPackage(), *It);
        if (!Enemy)
        {
            AddError(FString::Printf(TEXT("Could not construct %s to check its facing."), *Name));
            continue;
        }
        Enemy->ApplyBodyMesh();
        const float BodyOffDeg = DegreesBetween2D(Enemy->GetNamedBodyWorldForward(), Enemy->GetActorForwardVector());
        TestTrue(FString::Printf(TEXT("%s's body faces its actor's forward within %.0f deg (was %.1f)"), *Name, FacingToleranceDeg, BodyOffDeg),
            BodyOffDeg <= FacingToleranceDeg);
    }
    TestTrue(TEXT("At least one shipped mech was checked"), MeshesChecked > 0);
    return true;
}

#endif
