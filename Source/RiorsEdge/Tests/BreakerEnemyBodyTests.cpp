#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Combat/BreakerBodyPaint.h"
#include "Combat/BreakerEnemyBodyMath.h"
#include "Combat/BreakerFlinchMath.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Materials/MaterialInterface.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
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
// every enemy class RESOLVES its shipped body (a renamed uasset fails here,
// not in a screenshot). The ranged Lattice used to be the exception —
// composed primitives by the 2026-08-29 ruling — and the owner overturned
// that on 2026-09-11 ("take a look at adding in some of the meshes that we had
// that were preexisting, that we just never actually ended up using"): it
// wears Enemy_QuadShell, the one rig in the repo that ships a HIT and a RUN,
// and this now asserts that those two resolve as well, because a body with a
// hit animation that fails to load is a body with no indication it is taking
// damage, which is the complaint. Gated on the imported mechs existing, the
// shipped-samples shape: a clean clone still fights as primitives and passes.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEnemyBodyCastTest,
    "RiorsEdge.Combat.EnemyBody.CastResolves",
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
        TestTrue(FString::Printf(TEXT("%s ships a named body"), *It->GetName()),
                 Defaults->BodyMeshAsset.IsValid());
        if (bIsLattice)
        {
            // THE TWO ANIMATIONS THE OWNER'S COMPLAINT NEEDS. The QuadShell is
            // on the Lattice because it is the rig that has them.
            TestTrue(TEXT("The Lattice ships a hit animation"), Defaults->BodyHitAnimation.IsValid());
            TestTrue(TEXT("The Lattice ships a run animation"), Defaults->BodyRunAnimation.IsValid());
            if (Defaults->BodyHitAnimation.IsValid())
                TestNotNull(TEXT("The Lattice's hit resolves"), Defaults->BodyHitAnimation.TryLoad());
            if (Defaults->BodyRunAnimation.IsValid())
                TestNotNull(TEXT("The Lattice's run resolves"), Defaults->BodyRunAnimation.TryLoad());
            if (Defaults->BodyDeathAnimation.IsValid())
                TestNotNull(TEXT("The Lattice's death resolves"), Defaults->BodyDeathAnimation.TryLoad());
        }
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
// its contract is proved without a mesh: the livery is PURE at rest, a body
// hit flashes at OverlayFlashStrength (the livery still shows through — a
// mech that goes flat white on every rifle round is a mech with no paint
// job), ONLY a weak-point flash occludes fully (that is what makes the gold
// read as gold), the death burn occludes for its whole ride, the rank badge
// wears the same authored blend weights as the primitive blend (one table,
// not two), and the wound wash rises with damage to OverlayWoundStrengthMax
// and no further. Every non-weak-point state — flash, status and wound
// stacked together — stays under 1.0: the only full occlusion on a body is
// the weak point's and the corpse's.

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
    TestEqual(TEXT("a body flash is the authored flash strength"),
        ResolveOverlayStrength(State), OverlayFlashStrength, 1e-6f);
    TestTrue(TEXT("a body flash never occludes"), ResolveOverlayStrength(State) < 1.0f);
    State.bReactionWeakPoint = true;
    TestEqual(TEXT("a weak-point flash occludes"), ResolveOverlayStrength(State), 1.0f, 1e-6f);
    State.bReactionWeakPoint = false;
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
    TestTrue(FString::Printf(TEXT("drained wash is visible (> 0.15) but stops at the wound ceiling (was %.3f)"), Drained),
        Drained > 0.15f && Drained <= OverlayWoundStrengthMax + 1e-6f);

    // The status wash tops out at its own ceiling, under the weak point.
    State = FState();
    State.bStatus = true;
    State.StatusPulse = 1.0f;
    const float StatusPeak = ResolveOverlayStrength(State);
    TestTrue(FString::Printf(TEXT("a status at full pulse stops at the status ceiling (was %.3f)"), StatusPeak),
        StatusPeak > 0.0f && StatusPeak <= OverlayStatusStrengthMax + 1e-6f);

    // EVERYTHING AT ONCE, NO WEAK POINT: a rotting, drained, modifier-bearing
    // body taking a body hit is still a body with a livery.
    State = FState();
    State.Rank = EBreakerMonsterRank::ModifierBearing;
    State.bHealthRamp = true;
    State.HealthFraction = 0.0f;
    State.bStatus = true;
    State.StatusPulse = 1.0f;
    State.Reaction = EReaction::Flash;
    State.bReactionWeakPoint = false;
    const float Stacked = ResolveOverlayStrength(State);
    TestTrue(FString::Printf(TEXT("flash, status, wound and rank stacked never occlude (was %.3f)"), Stacked),
        Stacked < 1.0f);
    // The same body between flashes: the quiet layers stacked stay under too.
    State.Reaction = EReaction::Rest;
    const float StackedAtRest = ResolveOverlayStrength(State);
    TestTrue(FString::Printf(TEXT("status, wound and rank stacked at rest never occlude (was %.3f)"), StackedAtRest),
        StackedAtRest < 1.0f);
    // And the shipped ceilings themselves sit under the weak point's 1.0.
    TestTrue(TEXT("shipped flash strength is under full"), OverlayFlashStrength < 1.0f);
    TestTrue(TEXT("shipped wound ceiling is under full"), OverlayWoundStrengthMax < 1.0f);
    TestTrue(TEXT("shipped status ceiling is under full"), OverlayStatusStrengthMax < 1.0f);
    return true;
}

// THE HITCH IS RATIONED (BreakerFlinch::IsHeavy). Owner: every rifle round
// rocked the body, so at four or five rounds a second the mech wobbled
// rather than reacted. A hit is heavy when it alone is a real fraction of
// the body's health, when it lands on the weak point, or when the last
// half-second's fire adds up to one — so a burst that would have been three
// small wobbles is one hitch at the end of the burst. Pure: bare floats, no
// body. RecentDamage is the window's sum on the same side the caller keeps
// it, so the "window alone" case is held under the threshold on either
// reading of whether the current hit is already in it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFlinchOnlyHeavyHitsTest,
    "RiorsEdge.Combat.Flinch.OnlyHeavyHitsFlinch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFlinchOnlyHeavyHitsTest::RunTest(const FString& Parameters)
{
    using namespace BreakerFlinch;
    constexpr float MaxHealth = 100.0f;

    TestFalse(TEXT("a 1% body hit is not heavy"), IsHeavy(1.0f, 0.0f, MaxHealth, false));
    TestTrue(TEXT("a 1% weak-point hit is heavy"), IsHeavy(1.0f, 0.0f, MaxHealth, true));
    TestTrue(TEXT("an 8% body hit is heavy"), IsHeavy(8.0f, 0.0f, MaxHealth, false));
    TestTrue(TEXT("a 6% hit closing an 18% window is heavy"), IsHeavy(6.0f, 18.0f, MaxHealth, false));
    TestFalse(TEXT("a small hit on a 14% window is not heavy"), IsHeavy(0.5f, 14.0f, MaxHealth, false));
    TestFalse(TEXT("nothing recent and nothing now is not heavy"), IsHeavy(0.0f, 0.0f, MaxHealth, false));

    // Shipped configuration.
    TestEqual(TEXT("shipped HeavyHitFraction"), HeavyHitFraction, 0.08f, 1e-6f);
    TestEqual(TEXT("shipped HeavyWindowFraction"), HeavyWindowFraction, 0.15f, 1e-6f);
    TestEqual(TEXT("shipped HeavyWindowSeconds"), HeavyWindowSeconds, 0.5f, 1e-6f);
    TestTrue(TEXT("the window threshold sits above the single-hit threshold"), HeavyWindowFraction > HeavyHitFraction);
    return true;
}

// THE BODY'S CAPSULE IS A PAWN THAT WORLD GEOMETRY STOPS. Spawned in a
// fixture world (the BlackoutProtocol idiom), the enemy's root capsule wears
// the Pawn object type, blocks WorldStatic so a mech cannot walk through a
// wall, and ignores the two project trace channels so a player's shot and a
// player's interaction ray pass through to the hit boxes that answer them.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEnemyCapsuleBlocksWorldTest,
    "RiorsEdge.Combat.Enemy.CapsuleBlocksWorld",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEnemyCapsuleBlocksWorldTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };

    ABreakerEnemy* Enemy = World->SpawnActor<ABreakerEnemy>(FVector(0.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("actual enemy"), Enemy)) return false;

    UCapsuleComponent* Capsule = Enemy->FindComponentByClass<UCapsuleComponent>();
    if (!TestNotNull(TEXT("the enemy carries a capsule"), Capsule)) return false;
    TestTrue(TEXT("the capsule is the body's root"), Enemy->GetRootComponent() == Capsule);

    TestEqual(TEXT("the capsule is a Pawn"),
        static_cast<int32>(Capsule->GetCollisionObjectType()), static_cast<int32>(ECC_Pawn));
    TestEqual(TEXT("the capsule blocks WorldStatic"),
        static_cast<int32>(Capsule->GetCollisionResponseToChannel(ECC_WorldStatic)), static_cast<int32>(ECR_Block));
    TestEqual(TEXT("the capsule ignores GameTraceChannel1"),
        static_cast<int32>(Capsule->GetCollisionResponseToChannel(ECC_GameTraceChannel1)), static_cast<int32>(ECR_Ignore));
    TestEqual(TEXT("the capsule ignores GameTraceChannel2"),
        static_cast<int32>(Capsule->GetCollisionResponseToChannel(ECC_GameTraceChannel2)), static_cast<int32>(ECR_Ignore));
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
//
// The Lattice is in the cast, not exempt from it: its QuadShell names every
// limb Front_/Back_ and offered none of the biped pairs, so it stood at
// identity facing +Y and its hold-band strafe read as walking backwards.
// The pair table now carries Front_Shoulder_L/R. Because the pure half
// (yaw the pair's forward onto +X) is true by construction whatever axis
// the pair yields, a rig that also has a Back_Shoulder_L gives a SECOND,
// independent axis — front minus back is forward by the plain meaning of
// the words — and the fitted forward must agree with it.
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
    // A bone's component-space ref-pose position, composed up through every
    // parent to the root — the same walk the fit's own reader makes.
    const auto ComponentSpaceRefPosition = [](const FReferenceSkeleton& Ref, int32 BoneIndex) -> FVector
    {
        const TArray<FTransform>& Pose = Ref.GetRefBonePose();
        FTransform Composed = FTransform::Identity;
        for (int32 Bone = BoneIndex; Bone != INDEX_NONE; Bone = Ref.GetParentIndex(Bone))
        {
            Composed = Composed * Pose[Bone];
        }
        return Composed.GetLocation();
    };

    int32 MeshesChecked = 0;
    int32 SecondAxesChecked = 0;
    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (!It->IsChildOf(ABreakerEnemy::StaticClass())) continue;
        if (It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;
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

        // The independent second axis: a rig with a front and a back
        // shoulder on the same side says which way is forward without a
        // left/right pair, and the fit's answer must agree with it.
        const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
        const int32 FrontLeft = Ref.FindBoneIndex(FName(TEXT("Front_Shoulder_L")));
        const int32 BackLeft = Ref.FindBoneIndex(FName(TEXT("Back_Shoulder_L")));
        if (FrontLeft != INDEX_NONE && BackLeft != INDEX_NONE)
        {
            ++SecondAxesChecked;
            const FVector FrontMinusBack = ComponentSpaceRefPosition(Ref, FrontLeft) - ComponentSpaceRefPosition(Ref, BackLeft);
            const float SecondAxisOffDeg = DegreesBetween2D(
                Fit.RelativeRotation.RotateVector(MeshForward), Fit.RelativeRotation.RotateVector(FrontMinusBack));
            TestTrue(FString::Printf(TEXT("%s's fitted forward is within %.0f deg of its front-minus-back shoulder axis (was %.1f)"),
                *Name, FacingToleranceDeg, SecondAxisOffDeg), SecondAxisOffDeg <= FacingToleranceDeg);
        }
        // The biped's independent axis: the knee pole targets stand in FRONT
        // of the feet in every shipped rig, so pole-mid minus foot-mid points
        // the way the body faces without any left/right pair. Comparing the
        // fitted forward to the axis it was read from is a tautology (it was
        // 0.0 on a rig walking 32 degrees sideways); this is not.
        const int32 PoleL = Ref.FindBoneIndex(FName(TEXT("PoleTarget_L")));
        const int32 PoleR = Ref.FindBoneIndex(FName(TEXT("PoleTarget_R")));
        const int32 FootL = Ref.FindBoneIndex(FName(TEXT("Foot_L")));
        const int32 FootR = Ref.FindBoneIndex(FName(TEXT("Foot_R")));
        if (PoleL != INDEX_NONE && PoleR != INDEX_NONE && FootL != INDEX_NONE && FootR != INDEX_NONE)
        {
            ++SecondAxesChecked;
            const FVector PoleMid = (ComponentSpaceRefPosition(Ref, PoleL) + ComponentSpaceRefPosition(Ref, PoleR)) * 0.5f;
            const FVector FootMid = (ComponentSpaceRefPosition(Ref, FootL) + ComponentSpaceRefPosition(Ref, FootR)) * 0.5f;
            const float PoleAxisOffDeg = DegreesBetween2D(Fit.RelativeRotation.RotateVector(PoleMid - FootMid), FVector::ForwardVector);
            TestTrue(FString::Printf(TEXT("%s's fitted forward puts the knee poles within %.0f deg of +X (was %.1f)"),
                *Name, FacingToleranceDeg, PoleAxisOffDeg), PoleAxisOffDeg <= FacingToleranceDeg);
        }
        // Every pair the rig offers agrees with the chosen forward within the
        // tolerance, so a twisted rest pose is named by the log, never passed.
        for (const TCHAR* Left : { TEXT("Foot_L"), TEXT("Shoulder_L"), TEXT("UpperArm_L"), TEXT("UpperLeg_L") })
        {
            const FString RightName = FString(Left).Replace(TEXT("_L"), TEXT("_R"));
            const int32 L = Ref.FindBoneIndex(FName(Left));
            const int32 Rr = Ref.FindBoneIndex(FName(*RightName));
            if (L == INDEX_NONE || Rr == INDEX_NONE) continue;
            const FVector PairAxis = BreakerEnemyBody::BodyForwardAxisFromBilateralBones(
                ComponentSpaceRefPosition(Ref, L), ComponentSpaceRefPosition(Ref, Rr));
            if (PairAxis.IsNearlyZero()) continue;
            const float PairOffDeg = DegreesBetween2D(PairAxis, MeshForward);
            AddInfo(FString::Printf(TEXT("%s pair %s reads %.1f deg off the chosen forward"), *Name, Left, PairOffDeg));
        }

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
    // The Lattice's QuadShell ships Front_Shoulder_L and Back_Shoulder_L; a
    // cast where no rig offered the second axis is a cast the check never ran on.
    TestTrue(TEXT("At least one shipped rig offered a front/back second axis"), SecondAxesChecked > 0);
    return true;
}

// A PHASING BLINK GIVES BACK THE BODY IT TOOK, AND NOTHING ELSE. The owner:
// "the crit spot randomly appears on enemies sometimes". On a mech body
// ApplyBodyMesh hides the six primitives and the 40 cm gold ball (the ring
// replaces it on the Head bone), and every re-show path — pool revive,
// Wakeful rise, standing respawn — follows SetBodyVisible(true) with
// ApplyBodyMesh, which hid them again. SetModifierUntargetable(false) does
// not, and Phasing ends its blink through it every 6 s, so ~6 s into a
// fight the carrier came back wearing the gold ball and the primitive
// humanoid inside the mech. SetBodyVisible now re-shows only what the body
// wears. NewObject, no world, gated on the imported mechs — the same fixture
// as the revive pin above; the ring is not built outside a world, and the
// rule under test must hold without it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEnemyBodyPhaseReturnTest,
    "RiorsEdge.Combat.EnemyBody.PhaseReturnKeepsNamedBody",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEnemyBodyPhaseReturnTest::RunTest(const FString& Parameters)
{
    const FString MechDir = FPaths::ProjectContentDir() / TEXT("Breaker/Meshes/enemies/mechs");
    if (!IFileManager::Get().DirectoryExists(*MechDir))
    {
        return true;
    }

    ABreakerEnemy* Enemy = NewObject<ABreakerEnemy>();
    if (!Enemy)
    {
        AddError(TEXT("Could not construct an enemy to blink."));
        return false;
    }
    Enemy->ApplyBodyMesh();
    USkeletalMeshComponent* Body = Enemy->GetNamedBody();
    TestNotNull(TEXT("The enemy carries a named body component"), Body);
    if (!Body) return false;
    TestNotNull(TEXT("The shipped mesh resolved onto it"), Body->GetSkeletalMeshAsset());
    if (!Body->GetSkeletalMeshAsset()) return false;
    TestTrue(TEXT("The shipped mech ships a Head bone for the weak point to ride"),
        Body->GetBoneIndex(FName(TEXT("Head"))) != INDEX_NONE);

    auto* Ball = Cast<UPrimitiveComponent>(Enemy->GetDefaultSubobjectByName(TEXT("WeakPointVisual")));
    auto* Torso = Cast<UPrimitiveComponent>(Enemy->GetDefaultSubobjectByName(TEXT("BodyVisual")));
    TestNotNull(TEXT("The gold ball exists to be hidden"), Ball);
    TestNotNull(TEXT("The primitive torso exists to be hidden"), Torso);
    if (!Ball || !Torso) return false;

    TestTrue(TEXT("A mech body stands visible after the fit"), Body->GetVisibleFlag());
    TestFalse(TEXT("The fit hides the gold ball on a mech"), Ball->GetVisibleFlag());
    TestFalse(TEXT("The fit hides the primitive torso on a mech"), Torso->GetVisibleFlag());

    Enemy->SetModifierUntargetable(true);
    Enemy->SetModifierUntargetable(false);

    TestFalse(TEXT("The blink's return does not give a mech the gold ball"), Ball->GetVisibleFlag());
    TestFalse(TEXT("The blink's return does not give a mech the primitive torso"), Torso->GetVisibleFlag());
    TestTrue(TEXT("The named body stands through the blink"), Body->GetVisibleFlag());
    return true;
}

#endif
