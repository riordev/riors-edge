#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Tests/BreakerGuestDeathObserver.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerGuestDeathPresentationTest,
    "RiorsEdge.Multiplayer.GuestDeathPresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerGuestDeathPresentationTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
        true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Isolated presentation world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    APlayerController* PC = World->SpawnActor<APlayerController>();
    if (!Player || !PC) return false;
    Player->SetActorTickEnabled(false);
    PC->Player = NewObject<ULocalPlayer>(GEngine);
    PC->SetAsLocalPlayerController();
    PC->Possess(Player);
    auto* ASC = Player->GetAbilitySystemComponent();
    auto* Attr = Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player, Player);
    ASC->AddAttributeSetSubobject(Attr);
    Player->GetCombat()->BindAttributes(Attr);
    Player->SetActorLocation(FVector(0, 0, 500));
    Player->EnableInput(PC);
    UBreakerGuestDeathObserver* Observer = NewObject<UBreakerGuestDeathObserver>(Player);
    Player->GetCombat()->OnDeath.AddDynamic(Observer, &UBreakerGuestDeathObserver::OnDeath);
    Player->GetCombat()->OnVitalsRestored.AddDynamic(Observer, &UBreakerGuestDeathObserver::OnRestore);
    Player->SetRole(ROLE_AutonomousProxy);
    if (!TestTrue(TEXT("Fixture is locally controlled but not authority"), Player->IsLocallyControlled() && !Player->HasAuthority())) return false;
    UFunction* Notify = Attr->FindFunction(TEXT("OnRep_Health"));
    if (!TestNotNull(TEXT("Actual GAS Health rep-notify"), Notify)) return false;
    auto ReceiveHealth = [&](float Value)
    {
        struct FNotifyParameters { FGameplayAttributeData OldValue; } Params{Attr->Health};
        Attr->Health.SetBaseValue(Value);
        Attr->Health.SetCurrentValue(Value);
        Attr->ProcessEvent(Notify, &Params); // Actual GAMEPLAYATTRIBUTE_REPNOTIFY/ASC path.
    };
    ReceiveHealth(0);
    const FVector DeathAt = Player->GetActorLocation();
    Player->Tick(.1f);
    TestTrue(TEXT("Replicated zero health starts local presentation"), Player->IsAwaitingRespawn());
    TestFalse(TEXT("Guest gameplay input is disabled"), Player->InputEnabled());
    const float FirstLower = Player->GetDeathWeaponLowerFraction();
    ReceiveHealth(0); // Duplicate health notification must not restart the fall.
    Player->Tick(.1f);
    TestTrue(TEXT("Duplicate zero does not restart the existing beat"), Player->GetDeathWeaponLowerFraction() > FirstLower);
    Player->Tick(BreakerDeathBeat::TotalSeconds(Player->DeathBeat) + 1);
    TestTrue(TEXT("Elapsed local timer cannot revive guest"), Player->IsAwaitingRespawn());
    TestFalse(TEXT("Input remains disabled beyond the ordinary respawn beat"), Player->InputEnabled());
    TestEqual(TEXT("Held dead presentation stays lowered"), Player->GetDeathWeaponLowerFraction(), 1.0f);
    TestEqual(TEXT("No local vitals grant"), Attr->GetHealth(), 0.0f);
    TestTrue(TEXT("No local teleport"), Player->GetActorLocation().Equals(DeathAt));
    ReceiveHealth(Attr->GetMaxHealth());
    Player->Tick(.01f);
    TestFalse(TEXT("Authoritative health revival releases presentation"), Player->IsAwaitingRespawn());
    TestTrue(TEXT("Guest input resumes on health revival"), Player->InputEnabled());
    Player->Tick(Player->DeathBeat.FadeInSeconds + 1);
    TestEqual(TEXT("Revived presentation finishes at ready pose"), Player->GetDeathWeaponLowerFraction(), 0.0f);
    TestEqual(TEXT("No client death gameplay delegate broadcast"), Observer->Deaths, 0);
    TestEqual(TEXT("No client vitals-restored gameplay delegate broadcast"), Observer->Restores, 0);
    // A server owner still requires its actual death delegate/timer; the new
    // replicated-health presentation consumer must not create one on authority.
    Player->SetRole(ROLE_Authority);
    ReceiveHealth(0);
    Player->Tick(.1f);
    TestFalse(TEXT("Authority is excluded from client reconciliation"), Player->IsAwaitingRespawn());
    TestTrue(TEXT("Authority input is not changed by this bridge"), Player->InputEnabled());
    Player->GetCombat()->OnDeath.RemoveAll(Observer);
    Player->GetCombat()->OnVitalsRestored.RemoveAll(Observer);
    AddInfo(TEXT("Actual Health rep-notify and autonomous-proxy local presentation tested; packet transport/camera capture remains a two-process check."));
    return true;
}
#endif
