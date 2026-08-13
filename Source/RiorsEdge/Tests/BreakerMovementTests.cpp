#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/BreakerCharacter.h"
#include "Game/BreakerGameMode.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "UI/BreakerPlaytestHUD.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMovementStateTest,
    "RiorsEdge.Movement.StateSafety",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMovementStateTest::RunTest(const FString& Parameters)
{
    UBreakerCharacterMovementComponent* Movement = NewObject<UBreakerCharacterMovementComponent>();
    TestFalse(TEXT("Movement starts outside sprint"), Movement->IsSprinting());
    TestFalse(TEXT("Movement starts outside slide"), Movement->IsSliding());
    TestFalse(TEXT("Movement starts outside wall ride"), Movement->IsWallRiding());
    TestEqual(TEXT("Baseline gravity keeps jump arcs responsive"), Movement->GravityScale, 1.35f);

    Movement->SetSprinting(true);
    TestTrue(TEXT("Sprint request changes max speed state"), Movement->IsSprinting());
    TestEqual(TEXT("Sprint uses the configured sprint ceiling"), Movement->GetMaxSpeed(), Movement->SprintSpeed);
    Movement->SetSprinting(false);
    TestEqual(TEXT("Walking uses the configured walk ceiling"), Movement->GetMaxSpeed(), Movement->WalkSpeed);

    TestFalse(TEXT("Slide cannot start without a grounded character"), Movement->BeginSlide());
    Movement->SetSlideRequested(true);
    TestTrue(TEXT("Airborne slide input remains requested until landing"), Movement->IsSlideRequested());
    Movement->SetSlideRequested(false);
    TestFalse(TEXT("Releasing slide clears the landing request"), Movement->IsSlideRequested());
    TestFalse(TEXT("Wall jump cannot start outside a wall ride"), Movement->TryWallJump());
    TestFalse(TEXT("Dash safely rejects a component without a world"), Movement->TryDash(FVector::ForwardVector));
    TestEqual(TEXT("Dash has the requested four-second cooldown"), Movement->DashCooldown, 4.0f);
    TestEqual(TEXT("Slide boost has an anti-spam cooldown"), Movement->SlideBoostCooldown, 1.2f);
    TestEqual(TEXT("Momentum retains a hard safety ceiling"), Movement->MomentumHardCap, 4200.0f);
    TestFalse(TEXT("Redirect safely rejects a standing component"), Movement->TryRedirect(FVector::ForwardVector));
    TestFalse(TEXT("Redirect rejects a zero direction"), Movement->TryRedirect(FVector::ZeroVector));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMovementRedirectTest,
    "RiorsEdge.Movement.Redirect",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMovementRedirectTest::RunTest(const FString& Parameters)
{
    // Master 5.4 guardrail, written as a test rather than a comment: a redirect
    // may never increase horizontal speed.
    const float Walk = 700.0f;

    TestFalse(TEXT("A standing player cannot redirect"), UBreakerCharacterMovementComponent::CanRedirect(0.0f, Walk));
    TestFalse(TEXT("Below walk speed cannot redirect"), UBreakerCharacterMovementComponent::CanRedirect(699.9f, Walk));
    TestTrue(TEXT("Exactly walk speed can redirect"), UBreakerCharacterMovementComponent::CanRedirect(Walk, Walk));
    TestTrue(TEXT("Sprinting can redirect"), UBreakerCharacterMovementComponent::CanRedirect(1400.0f, Walk));

    // A 180-degree reversal keeps the magnitude exactly.
    const FVector Sprinting(1400.0f, 0.0f, 0.0f);
    const FVector Reversed = UBreakerCharacterMovementComponent::RedirectHorizontalVelocity(Sprinting, -FVector::ForwardVector, Walk);
    TestTrue(TEXT("A reversal preserves speed exactly"), FMath::IsNearlyEqual(Reversed.Size(), 1400.0f, 0.01f));
    TestTrue(TEXT("A reversal actually reverses"), Reversed.GetSafeNormal().Equals(-FVector::ForwardVector, KINDA_SMALL_NUMBER));
    TestTrue(TEXT("A redirect stays horizontal"), FMath::IsNearlyZero(Reversed.Z));

    // A lateral redirect neither gains nor loses speed.
    const FVector Lateral = UBreakerCharacterMovementComponent::RedirectHorizontalVelocity(Sprinting, FVector::RightVector, Walk);
    TestTrue(TEXT("A lateral redirect preserves speed"), FMath::IsNearlyEqual(Lateral.Size(), 1400.0f, 0.01f));
    TestTrue(TEXT("A lateral redirect changes heading"), Lateral.GetSafeNormal().Equals(FVector::RightVector, KINDA_SMALL_NUMBER));

    // A non-normalized direction must not scale the result — this is exactly
    // how a redirect would accidentally become a dash.
    const FVector Overlong = UBreakerCharacterMovementComponent::RedirectHorizontalVelocity(Sprinting, FVector(0.0f, 50.0f, 0.0f), Walk);
    TestTrue(TEXT("Direction magnitude does not leak into speed"), FMath::IsNearlyEqual(Overlong.Size(), 1400.0f, 0.01f));

    // A vertical-only direction is not a redirect at all: velocity is untouched.
    const FVector Vertical = UBreakerCharacterMovementComponent::RedirectHorizontalVelocity(Sprinting, FVector::UpVector, Walk);
    TestTrue(TEXT("A vertical direction leaves velocity alone"), Vertical.Equals(Sprinting, KINDA_SMALL_NUMBER));

    // The floor keeps a redirect from dead-stopping, and never manufactures
    // speed above the input.
    const FVector Slow(300.0f, 0.0f, 0.0f);
    const FVector Floored = UBreakerCharacterMovementComponent::RedirectHorizontalVelocity(Slow, FVector::RightVector, Walk);
    TestTrue(TEXT("The floor prevents a dead stop"), FMath::IsNearlyEqual(Floored.Size(), Walk, 0.01f));
    // Vertical velocity is not the redirect's business.
    const FVector Airborne(1000.0f, 0.0f, 400.0f);
    const FVector RedirectedAir = UBreakerCharacterMovementComponent::RedirectHorizontalVelocity(Airborne, FVector::RightVector, Walk);
    TestTrue(TEXT("Only the horizontal component is considered"), FMath::IsNearlyEqual(RedirectedAir.Size(), 1000.0f, 0.01f));
    TestTrue(TEXT("A redirect adds no vertical velocity"), FMath::IsNearlyZero(RedirectedAir.Z));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMovementSpeedMultiplierTest,
    "RiorsEdge.Movement.SpeedMultiplier",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMovementSpeedMultiplierTest::RunTest(const FString& Parameters)
{
    // Multiplicative composition, matching how gear and tree multipliers
    // already compose.
    TestEqual(TEXT("Nothing pushed composes to one"), UBreakerCharacterMovementComponent::ComposeSpeedMultipliers({}), 1.0f);
    TestTrue(TEXT("Two multipliers multiply rather than add"),
        FMath::IsNearlyEqual(UBreakerCharacterMovementComponent::ComposeSpeedMultipliers({ 1.2f, 1.5f }), 1.8f, 0.0001f));
    TestTrue(TEXT("A non-positive multiplier is ignored rather than zeroing speed"),
        FMath::IsNearlyEqual(UBreakerCharacterMovementComponent::ComposeSpeedMultipliers({ 1.5f, 0.0f, -2.0f }), 1.5f, 0.0001f));

    UBreakerCharacterMovementComponent* Movement = NewObject<UBreakerCharacterMovementComponent>();
    TestEqual(TEXT("No multiplier is pushed by default"), Movement->GetSpeedMultiplier(), 1.0f);

    Movement->PushSpeedMultiplier(TEXT("Window.Swift.Overdrive"), 1.1f, 8.0f);
    TestTrue(TEXT("A pushed multiplier applies"), FMath::IsNearlyEqual(Movement->GetSpeedMultiplier(), 1.1f, 0.0001f));
    TestTrue(TEXT("The multiplier reaches the speed cap"),
        FMath::IsNearlyEqual(Movement->GetMaxSpeed(), Movement->WalkSpeed * 1.1f, 0.01f));

    // Pushing the same key replaces rather than stacking, so a re-cast cannot
    // compound its own buff.
    Movement->PushSpeedMultiplier(TEXT("Window.Swift.Overdrive"), 1.1f, 8.0f);
    TestTrue(TEXT("Re-pushing a key does not stack"), FMath::IsNearlyEqual(Movement->GetSpeedMultiplier(), 1.1f, 0.0001f));

    Movement->PushSpeedMultiplier(TEXT("Debuff.Slow"), 0.5f, 3.0f);
    TestTrue(TEXT("Distinct keys compose"), FMath::IsNearlyEqual(Movement->GetSpeedMultiplier(), 0.55f, 0.0001f));

    Movement->PopSpeedMultiplier(TEXT("Debuff.Slow"));
    TestTrue(TEXT("Popping removes only its own key"), FMath::IsNearlyEqual(Movement->GetSpeedMultiplier(), 1.1f, 0.0001f));
    Movement->PopSpeedMultiplier(TEXT("Window.Swift.Overdrive"));
    TestEqual(TEXT("Popping the last key restores the baseline"), Movement->GetSpeedMultiplier(), 1.0f);

    Movement->PushSpeedMultiplier(NAME_None, 2.0f, 1.0f);
    Movement->PushSpeedMultiplier(TEXT("Bad"), 0.0f, 1.0f);
    TestEqual(TEXT("Invalid pushes are refused"), Movement->GetSpeedMultiplier(), 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerPlaytestAssemblyTest,
    "RiorsEdge.Playtest.Assembly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPlaytestAssemblyTest::RunTest(const FString& Parameters)
{
    const ABreakerGameMode* GameMode = GetDefault<ABreakerGameMode>();
    TestNotNull(TEXT("Playtest game mode has a pawn class"), GameMode->DefaultPawnClass.Get());
    if (GameMode->DefaultPawnClass)
    {
        TestTrue(TEXT("Default pawn remains a C++ BreakerCharacter descendant"),
            GameMode->DefaultPawnClass->IsChildOf(ABreakerCharacter::StaticClass()));
        TestEqual(TEXT("Editor-authored Breaker Blueprint is the active pawn"),
            GameMode->DefaultPawnClass->GetPathName(),
            FString(TEXT("/Game/ProjectBreaker/Characters/BP_BreakerCharacter.BP_BreakerCharacter_C")));
        const ABreakerCharacter* CharacterDefaults = Cast<ABreakerCharacter>(GameMode->DefaultPawnClass->GetDefaultObject());
        TestNotNull(TEXT("Active pawn exposes Breaker defaults"), CharacterDefaults);
        if (CharacterDefaults)
        {
            TestEqual(TEXT("Baseline supports a double jump"), CharacterDefaults->JumpMaxCount, 2);
            TestEqual(TEXT("Baseline look sensitivity remains one-to-one"), CharacterDefaults->GetLookSensitivity(), 1.0f);
        }
    }
    TestEqual(TEXT("Playtest HUD remains active"), GameMode->HUDClass.Get(), ABreakerPlaytestHUD::StaticClass());
    return true;
}

#endif
