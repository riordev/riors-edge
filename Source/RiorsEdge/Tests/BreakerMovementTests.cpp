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
