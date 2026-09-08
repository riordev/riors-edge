#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Overdrive.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerBuildConditions.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Weapons/BreakerWeaponComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerOverdriveFloorRuntimeTest,
    "RiorsEdge.Abilities.OverdrivePaidEffectiveFloor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerOverdriveFloorRuntimeTest::RunTest(const FString& Parameters)
{
    for (int32 Scenario = 0; Scenario < 3; ++Scenario)
    {
        UWorld::InitializationValues Init;
        Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
        if (!World) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());
        const uint64 SavedFrame = GFrameCounter;
        ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = SavedFrame; };
        auto* Player = World->SpawnActor<ABreakerCharacter>();
        if (!Player) return false;
        Player->SetActorTickEnabled(false);
        auto* Movement = Player->GetBreakerMovement(); Movement->SetComponentTickEnabled(false);
        auto* ASC = Player->GetAbilitySystemComponent(); auto* Attributes = Player->GetAttributes();
        ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attributes);
        Player->GetCombat()->BindAttributes(Attributes); Player->GetProgression()->BindAttributes(Attributes);
        if (!TestTrue(TEXT("actual Swift class"), Player->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Swift))) return false;
        auto* Momentum = Player->GetMomentum(); Momentum->BindAttributes(Attributes); Momentum->SetComponentTickEnabled(false);
        Movement->SetMovementMode(MOVE_Walking);
        const auto BaselineChannels = Player->GetWeapon()->GetShotChannels();
        // Native ground-speed income, without direct resource grants or node tags.
        Movement->Velocity = FVector(Movement->WalkSpeed, 0, 0);
        for (int32 Second = 0; Second < 40; ++Second)
        {
            Player->SetActorLocation(Player->GetActorLocation() + Movement->Velocity);
            Momentum->AdvanceLoop(1);
        }
        if (!TestEqual(TEXT("normal moving income funds ultimate"), Momentum->GetMomentum(), 100.0f)) return false;
        Movement->StopMovementImmediately();
        const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Overdrive::StaticClass(), 1));
        if (!TestTrue(TEXT("actual paid Overdrive"), ASC->TryActivateAbility(Handle))) return false;
        TestEqual(TEXT("raw resource remains spent"), Momentum->GetMomentum(), 0.0f);
        TestEqual(TEXT("raw fraction remains zero"), Momentum->GetMomentumFraction(), 0.0f);
        TestEqual(TEXT("effective band immediately Redline"), Momentum->GetMomentumState(), EBreakerMomentumState::Redline);
        TestTrue(TEXT("resource-low condition still reads empty raw bar"), FBreakerBuildConditionState::EvaluateForActor(Player).IsActive(EBreakerBuildCondition::ResourceLow));
        const auto ActiveChannels = Player->GetWeapon()->GetShotChannels();
        TestTrue(TEXT("actual weapon channels receive Redline treatment"), ActiveChannels.PierceCount > BaselineChannels.PierceCount || ActiveChannels.ChainCount > BaselineChannels.ChainCount);
        auto* State = UBreakerAbilityStateComponent::FindOrAdd(Player);
        ++GFrameCounter; World->Tick(LEVELTICK_All, .2f);
        const float RemainingBeforeSecondPress = State->GetWindowRemaining(UBreakerAbility_Overdrive::WindowKey());
        ASC->TryActivateAbility(Handle);
        TestEqual(TEXT("second press cannot refill or pay from effective floor"), Momentum->GetMomentum(), 0.0f);
        TestEqual(TEXT("second press cannot refresh the paid window"), State->GetWindowRemaining(UBreakerAbility_Overdrive::WindowKey()), RemainingBeforeSecondPress);
        if (Scenario == 1) State->CloseWindow(UBreakerAbility_Overdrive::WindowKey());
        else if (Scenario == 2)
        {
            FBreakerDamageRequest Hit; Hit.BaseDamage = 100000; Hit.bCanCritical = false; Hit.bCanBeAvoided = false;
            Hit.DamageFamily = EBreakerDamageFamily::TrueDamage;
            Player->GetCombat()->ReceiveDamage(Hit);
            TestTrue(TEXT("actual death clears floor"), Player->GetCombat()->IsDead());
            Player->GetCombat()->RestoreVitals();
        }
        else for (int32 Step = 0; Step < 180; ++Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); }
        TestEqual(TEXT("closed window cannot leave effective Redline"), Momentum->GetMomentumState(), EBreakerMomentumState::Settled);
        TestEqual(TEXT("floor lifecycle never changed spendable resource"), Momentum->GetMomentum(), 0.0f);
        const auto EndChannels = Player->GetWeapon()->GetShotChannels();
        TestEqual(TEXT("weapon pierce returns to raw band"), EndChannels.PierceCount, BaselineChannels.PierceCount);
        TestEqual(TEXT("weapon chain returns to raw band"), EndChannels.ChainCount, BaselineChannels.ChainCount);
    }
    return true;
}
#endif
