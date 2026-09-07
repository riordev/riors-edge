#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerAimTradeRuntimeTest,
    "RiorsEdge.Movement.AimTradeRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerAimTradeRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { GFrameCounter = InitialFrame; World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    Player->SetActorTickEnabled(false);
    auto* Movement = Player->GetBreakerMovement();
    Movement->SetComponentTickEnabled(false);
    Movement->SetMovementMode(MOVE_Walking);
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player);
    ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    auto* Weapon = Player->GetWeapon();
    Weapon->BeginPlay(); // Native weapon only; Character BeginPlay would load saves.
    auto Advance = [&]()
    {
        for (int32 Step = 0; Step < 30; ++Step)
        {
            ++GFrameCounter; World->Tick(LEVELTICK_All, .05f);
            if (!World->GetTimerManager().HasBeenTickedThisFrame()) World->GetTimerManager().Tick(.05f);
        }
    };
    float FirstPenalty = 0;
    for (const auto Archetype : { EBreakerWeaponArchetype::SMG, EBreakerWeaponArchetype::Sniper })
    {
        Weapon->EquipArchetype(Archetype);
        Advance();
        Weapon->SetAiming(false); Advance();
        Movement->Velocity = FVector::ZeroVector;
        TestEqual(TEXT("stationary first shot retains authored perfect accuracy"), Weapon->GetNextShotSpreadDegrees(), 0.0f);
        // Hold identical walking velocity for both measurements. Stationary
        // first shots intentionally forgive the base cone in both aim states;
        // movement is the authored accuracy cost that ADS increases.
        Movement->Velocity = FVector(200, 0, 0);
        const float HipSpread = Weapon->GetNextShotSpreadDegrees();
        TestTrue(TEXT("moving hip fire has an actual cone"), HipSpread > 0);
        const float HipSpeed = Movement->GetMaxSpeed();
        TestTrue(TEXT("native walking has positive speed"), HipSpeed > 0);
        Weapon->SetAiming(true);
        Advance();
        TestEqual(TEXT("actual world clock completes aim transition"), Weapon->GetAimAlpha(), 1.0f);
        TestTrue(TEXT("moving while aimed pays the authored larger cone"), Weapon->GetNextShotSpreadDegrees() > HipSpread);
        TestTrue(TEXT("aiming reduces actual movement speed"), Movement->GetMaxSpeed() < HipSpeed);
        const float Penalty = Movement->GetMaxSpeed() / HipSpeed;
        TestEqual(TEXT("movement consumes weapon aim penalty"), Penalty, Weapon->GetAimMoveSpeedMultiplier());
        if (Archetype == EBreakerWeaponArchetype::SMG) FirstPenalty = Penalty;
        else TestTrue(TEXT("sniper pays stronger movement cost than SMG"), Penalty < FirstPenalty);
        Weapon->SetAiming(false); Advance();
        TestEqual(TEXT("release completes aim-out"), Weapon->GetAimAlpha(), 0.0f);
        TestEqual(TEXT("release restores hip cone"), Weapon->GetNextShotSpreadDegrees(), HipSpread);
        TestEqual(TEXT("release restores native movement speed"), Movement->GetMaxSpeed(), HipSpeed);
    }
    return true;
}
#endif
