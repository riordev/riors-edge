#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Character.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponFeel.h"

// ---------------------------------------------------------------------------
// THE HIP-FIRE / ADS TRADE, BOTH SIDES
// ---------------------------------------------------------------------------
// This trade was authored across a boundary and sat half-built for a while:
// the weapons layer paid aim-in time and a movement SPREAD penalty, but there
// was no movement SPEED penalty, because Movement/ had no aim awareness. Hip
// fire was therefore strictly worse than aiming and the "trade" was not one.
//
// The weapons half authors the penalty per archetype and composes it against
// live aim progress. The movement half is exactly one consumer inside
// GetMaxSpeed. Neither side is meaningful alone, and the failure mode when a
// two-sided feature loses a side is silence — an honest number that nobody
// reads — so what needs pinning is the CONNECTION.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAimSpeedPenaltyTest,
    "RiorsEdge.Movement.AimSpeedPenalty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAimSpeedPenaltyTest::RunTest(const FString& Parameters)
{
    // ---- The weapons half, as pure maths -----------------------------------
    FBreakerRecoilProfile Profile;
    Profile.AimMoveSpeedMultiplier = 0.50f;

    // Not aiming is EXACTLY 1.0, not approximately. Anything else would be a
    // silent global speed retune dressed up as an aim feature.
    TestEqual(TEXT("Hip fire is bit-identical to no penalty at all"),
        FBreakerWeaponFeel::AimMoveSpeedMultiplier(Profile, 0.0f), 1.0f, 0.0f);

    // Fully aimed is the authored figure.
    TestEqual(TEXT("Fully aimed pays the archetype's authored penalty"),
        FBreakerWeaponFeel::AimMoveSpeedMultiplier(Profile, 1.0f), 0.50f, 0.0001f);

    // The penalty arrives at the pace the BENEFITS do. Snapping the full cost
    // on at the first frame of the aim-in would make ADS feel like being
    // bolted to the floor before it feels like accuracy.
    const float Halfway = FBreakerWeaponFeel::AimMoveSpeedMultiplier(Profile, 0.5f);
    TestTrue(TEXT("Half-aimed pays a partial cost"), Halfway > 0.50f && Halfway < 1.0f);

    // Monotonic: no point in the aim-in is faster than an earlier one.
    float Previous = 1.0f;
    for (int32 Step = 0; Step <= 20; ++Step)
    {
        const float Current = FBreakerWeaponFeel::AimMoveSpeedMultiplier(Profile, Step / 20.0f);
        TestTrue(TEXT("Aiming in never speeds the player up at any point"), Current <= Previous + UE_KINDA_SMALL_NUMBER);
        Previous = Current;
    }

    // A misauthored profile must not invert the trade into a speed BUFF. This
    // is clamped on BOTH sides of the boundary deliberately; a content author
    // typing 1.2 should get "no penalty", never "sprint faster while scoped".
    FBreakerRecoilProfile Misauthored;
    Misauthored.AimMoveSpeedMultiplier = 1.75f;
    TestTrue(TEXT("An over-1.0 authored value can never become a speed buff"),
        FBreakerWeaponFeel::AimMoveSpeedMultiplier(Misauthored, 1.0f) <= 1.0f + UE_KINDA_SMALL_NUMBER);

    // ---- The movement half, connected --------------------------------------
    // A movement component with no weapon component beside it returns exactly
    // 1.0, which is what keeps every bare test object and every pre-existing
    // call site bit-identical to before this landed.
    UBreakerCharacterMovementComponent* Bare = NewObject<UBreakerCharacterMovementComponent>();
    TestEqual(TEXT("No weapon component means no penalty, exactly"),
        Bare->GetAimSpeedMultiplier(), 1.0f, 0.0f);

    // With a weapon present and not aiming, still exactly 1.0 — so the
    // connection cannot cost anything until the player actually aims.
    // Constructed with the pawn as OUTER and deliberately NOT registered: the
    // house idiom for this suite is worldless objects, and RegisterComponent
    // needs a world. Naming the outer is enough — UActorComponent adds itself
    // to the owner's component list at construction, which is exactly what
    // FindComponentByClass walks.
    ACharacter* Pawn = NewObject<ACharacter>();
    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>(Pawn);
    UBreakerCharacterMovementComponent* Movement = NewObject<UBreakerCharacterMovementComponent>(Pawn);
    TestEqual(TEXT("A hip-firing player with a weapon is unaffected"),
        Movement->GetAimSpeedMultiplier(), 1.0f, 0.0001f);

    // And the movement layer reads the weapon rather than holding its own copy
    // — the whole point of the boundary. If these two ever disagree, someone
    // has cached the multiplier and it will go stale across a weapon swap.
    TestEqual(TEXT("Movement reads the weapon's number rather than its own"),
        Movement->GetAimSpeedMultiplier(),
        FMath::Clamp(Weapon->GetAimMoveSpeedMultiplier(), 0.0f, 1.0f), 0.0001f);

    return true;
}

#endif
