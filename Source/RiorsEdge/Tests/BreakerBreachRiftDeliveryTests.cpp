#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerTankAbilities.h"
#include "Abilities/BreakerBreachCharge.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerGritComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "TimerManager.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerBreachRiftDeliveryTest, "RiorsEdge.Abilities.BreachRiftDelivery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerBreachRiftDeliveryTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC = Player->GetAbilitySystemComponent(); ASC->InitAbilityActorInfo(Player, Player);
    ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    Player->GetProgression()->BindAttributes(Player->GetAttributes());
    if (!TestTrue(TEXT("actual Tank class selection"), Player->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Tank))) return false;
    Player->GetGrit()->BindAttributes(Player->GetAttributes());
    auto* Enemy = World->SpawnActor<ABreakerEnemy>(FVector(350, 0, 0), FRotator::ZeroRotator);
    if (!Enemy) return false;
    auto* Health = Cast<UBreakerAttributeSet>(Enemy->GetDefaultSubobjectByName(TEXT("Attributes")));
    if (!Health) return false;
    Enemy->GetAbilitySystemComponent()->AddAttributeSetSubobject(Health); Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
    Health->ApplyMaxHealth(10000); Health->ApplyHealth(10000);
    auto* Status = Enemy->FindComponentByClass<UBreakerStatusComponent>();
    auto* PlayerStatus = Player->FindComponentByClass<UBreakerStatusComponent>();
    if (!Status || !PlayerStatus) return false;
    Status->SetComponentTickEnabled(false); PlayerStatus->SetComponentTickEnabled(false);
    // Real incoming damage funds Grit through the actual Character callback,
    // normally bound by BeginPlay; no owner save or direct resource grant.
    FScriptDelegate DamageDelegate; DamageDelegate.BindUFunction(Player, TEXT("HandleClassResourceDamageTaken"));
    Player->GetCombat()->OnDamageTaken.Add(DamageDelegate);
    // Enter the ordinary combat state and refill its per-source budget before
    // damage events. Two real hits respect the authored 10 Grit/s damage cap.
    Player->GetGrit()->SetInCombat(true);
    Player->GetGrit()->AdvanceLoop(1.0f);
    FBreakerDamageRequest Incoming; Incoming.BaseDamage = Player->GetAttributes()->GetMaxHealth() * .2f;
    Incoming.bCanCritical = false; Incoming.SetInstigator(Enemy);
    for (int32 Hit = 0; Hit < 2; ++Hit)
    {
        TestTrue(TEXT("real incoming hit supplies the resource event"), Player->GetCombat()->ReceiveDamage(Incoming).HealthDamage > 0);
        Player->GetGrit()->AdvanceLoop(1.0f);
    }
    const float BeforeBlastHealth = Player->GetAttributes()->GetHealth();
    const float BeforeGrit = Player->GetGrit()->GetGrit();
    // Native grant isolates elemental delivery, not campaign acquisition.
    const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_BreachCharge::StaticClass(), 1));
    if (!TestTrue(TEXT("real paid Breach cast"), ASC->TryActivateAbility(Handle))) return false;
    TestTrue(TEXT("cast spends earned Grit"), Player->GetGrit()->GetGrit() < BeforeGrit);
    for (int32 Step = 0; Step < 32; ++Step)
    {
        ++GFrameCounter; World->Tick(LEVELTICK_All, .05f);
        if (!World->GetTimerManager().HasBeenTickedThisFrame()) World->GetTimerManager().Tick(.05f);
    }
    TestTrue(TEXT("actual fused blast damages enemy"), Health->GetHealth() < 10000);
    TestTrue(TEXT("actual enemy-facing blast builds Rift"), Status->GetRiftBuildup() > 0);
    TestEqual(TEXT("Breach does not build Entropy"), Status->GetEntropyBuildup(), 0.0f);
    TestTrue(TEXT("nearby blast still pays actual self-damage"), Player->GetAttributes()->GetHealth() < BeforeBlastHealth);
    TestEqual(TEXT("self-damage side does not inherit enemy Rift conversion"), PlayerStatus->GetRiftBuildup(), 0.0f);
    int32 Charges = 0; for (TActorIterator<ABreakerBreachCharge> It(World); It; ++It) ++Charges;
    TestEqual(TEXT("fuse cleans up actual charge actor"), Charges, 0);
    return true;
}
#endif
