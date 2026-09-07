#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Characters/BreakerCharacter.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "UI/BreakerDeathInput.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerInterruptedInputRuntimeTest,
    "RiorsEdge.Input.MenuAndDeathReleaseRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerInterruptedInputRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Isolated world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("Actual character"), Player)) return false;
    // Do not begin Character play: no save loading or persistence listeners.
    UBreakerWeaponComponent* Weapon = Player->GetWeapon();
    UBreakerCharacterMovementComponent* Movement = Player->GetBreakerMovement();
    if (!TestNotNull(TEXT("Weapon"), Weapon) || !TestNotNull(TEXT("Movement"), Movement)) return false;
    Weapon->ResetAmmunition();
    Weapon->SetAiming(true);
    Weapon->StartFire();
    TestTrue(TEXT("Held automatic trigger before menu"), Weapon->IsTriggerHeld());
    Player->ClearHeldGameplayInput(); // The menu entry's shared production operation.
    TestFalse(TEXT("Menu clears held trigger"), Weapon->IsTriggerHeld());
    TestFalse(TEXT("Menu disengages aiming, including toggle mode"), Weapon->IsAiming());
    const int32 AmmoAfterMenu = Weapon->GetMagazineAmmo();
    World->Tick(LEVELTICK_All, 0.15f);
    World->Tick(LEVELTICK_All, 0.15f);
    TestEqual(TEXT("Automatic timer does not resume after a swallowed release"), Weapon->GetMagazineAmmo(), AmmoAfterMenu);
    Movement->SetSprinting(true);
    Player->ClearHeldGameplayInput();
    TestFalse(TEXT("Menu clears sprint intent"), Movement->IsSprinting());
    Weapon->StartFire();
    Weapon->SetAiming(true);
    UFunction* Death = Player->FindFunction(FName(TEXT("HandlePlayerDeath")));
    if (!TestNotNull(TEXT("Actual reflected death handler"), Death)) return false;
    // Native reflected actor calls require initialized actors or the editor guard.
    // Keep save-loading BeginPlay outside this isolated input fixture.
    { TGuardValue<bool> AllowNativeActorEvent(GAllowActorScriptExecutionInEditor, true); Player->ProcessEvent(Death, nullptr); }
    TestFalse(TEXT("Real death entry clears fire before disabling input"), Weapon->IsTriggerHeld());
    TestFalse(TEXT("Real death entry clears aim"), Weapon->IsAiming());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerDeathConfirmationTest,
    "RiorsEdge.UI.DeathConfirmation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerDeathConfirmationTest::RunTest(const FString& Parameters)
{
    for (const FKey& Key : { EKeys::Enter, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom })
    {
        TestTrue(TEXT("Confirmation retries when allowed"), BreakerDeathInput::Confirm(Key, false, true, false) == EBreakerDeathAction::Retry);
        TestTrue(TEXT("Exhausted budget confirms Return instead"), BreakerDeathInput::Confirm(Key, false, false, false) == EBreakerDeathAction::Return);
        TestTrue(TEXT("Holding confirmation never repeats travel"), BreakerDeathInput::Confirm(Key, true, true, false) == EBreakerDeathAction::None);
        TestTrue(TEXT("Pending travel cannot dispatch twice"), BreakerDeathInput::Confirm(Key, false, true, true) == EBreakerDeathAction::None);
    }
    TestTrue(TEXT("Escape cannot accidentally retry a dead pawn"), BreakerDeathInput::Confirm(EKeys::Escape, false, true, false) == EBreakerDeathAction::None);
    return true;
}
#endif
