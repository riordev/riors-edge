#include "Misc/AutomationTest.h"
#include "Combat/BreakerHealthRegenMath.h"
#include "Combat/BreakerCombatComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerHealthRegenRuleTest,
    "RiorsEdge.Combat.HealthRegen", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHealthRegenRuleTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHealthRegen;
    const UBreakerCombatComponent* Defaults = GetDefault<UBreakerCombatComponent>();
    TestEqual(TEXT("shipped recovery rate"), Defaults->BaseHealthRegenPerSecond, 2.0f);
    TestEqual(TEXT("shipped recovery delay"), Defaults->BaseHealthRegenDelaySeconds, 4.0f);
    TestEqual(TEXT("no recovery before delay"), Step(50, 100, 3.99f, 1), 50.0f);
    TestEqual(TEXT("no elapsed recovery at boundary"), Step(50, 100, 4, 1), 50.0f);
    TestEqual(TEXT("crossing delay earns only elapsed recovery"), Step(50, 100, 4.25f, 1), 50.5f);
    TestEqual(TEXT("one quiet second pays two health"), Step(50, 100, 5, 1), 52.0f);
    TestEqual(TEXT("recovery caps at maximum"), Step(99, 100, 10, 1), 100.0f);
    TestEqual(TEXT("a corpse stays dead"), Step(0, 100, 10, 1), 0.0f);
    TestEqual(TEXT("full health stays full"), Step(100, 100, 10, 1), 100.0f);
    TestEqual(TEXT("paused time pays nothing"), Step(50, 100, 10, 0), 50.0f);
    TestEqual(TEXT("disabled rate pays nothing"), Step(50, 100, 10, 1, 0), 50.0f);
    TestEqual(TEXT("incoming hit restarts delay"), Step(50, 100, CombatAge(0, 10), 1), 50.0f);
    TestEqual(TEXT("outgoing hit restarts delay"), Step(50, 100, CombatAge(10, 0), 1), 50.0f);
    float Sliced = 50.0f;
    for (int32 Frame = 1; Frame <= 10; ++Frame) Sliced = Step(Sliced, 100, 3.5f + Frame * 0.1f, 0.1f);
    TestEqual(TEXT("delay crossing is frame-rate independent"), Sliced, Step(50, 100, 4.5f, 1), 0.0001f);

    const ABreakerCharacter* Character = GetDefault<ABreakerCharacter>();
    TestEqual(TEXT("missing world cannot establish quiet combat time"), Character->GetSecondsSinceCombat(), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerPlayerRecoveryTickTest,
    "RiorsEdge.Combat.PlayerRecoveryTick", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPlayerRecoveryTickTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Initialization;
    Initialization.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Initialization);
    if (!TestNotNull(TEXT("Transient recovery world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    // Registered world components exercise the same tick contract as gameplay.
    AActor* Dummy = World->SpawnActor<AActor>();
    UBreakerCombatComponent* DummyCombat = NewObject<UBreakerCombatComponent>(Dummy);
    Dummy->AddInstanceComponent(DummyCombat);
    DummyCombat->RegisterComponent();
    UBreakerAttributeSet* DummyAttributes = NewObject<UBreakerAttributeSet>(Dummy);
    DummyAttributes->ApplyHealth(50.0f);
    DummyCombat->BindAttributes(DummyAttributes);
    DummyCombat->TickComponent(1.0f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Non-player component does not regenerate"), DummyAttributes->GetHealth(), 50.0f);
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("Player pawn"), Player) || !TestNotNull(TEXT("Player controller"), Controller)) return false;
    Controller->Possess(Player);
    // The isolated world has no game mode to allocate the normal player state.
    Player->SetPlayerState(World->SpawnActor<APlayerState>());
    if (!TestTrue(TEXT("Recovery subject is player controlled"), Player->IsPlayerControlled())) return false;
    UBreakerCombatComponent* Combat = Player->GetCombat();
    UBreakerAttributeSet* Attributes = Player->GetAttributes();
    Combat->BindAttributes(Attributes);
    Attributes->ApplyHealth(50.0f);
    Combat->TickComponent(1.0f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("A quiet player actually recovers health"), Attributes->GetHealth(), 52.0f);

    FBreakerDamageRequest Hit;
    Hit.BaseDamage = 10.0f;
    Hit.bCanCritical = false;
    Hit.bBypassShield = true;
    Combat->ReceiveDamage(Hit);
    const float AfterHit = Attributes->GetHealth();
    TestTrue(TEXT("The player received the hit"), AfterHit < 52.0f);
    Combat->TickComponent(1.0f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("The real incoming-hit clock stops recovery"), Attributes->GetHealth(), AfterHit);
    Attributes->ApplyHealth(0.0f);
    Combat->TickComponent(1.0f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("The runtime tick does not revive a corpse"), Attributes->GetHealth(), 0.0f);
    return true;
}

#endif
