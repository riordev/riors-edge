#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Components/SceneComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerStatusRules.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerVoidStatusRuntimeTest, "RiorsEdge.Combat.VoidStatusRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerVoidStatusRuntimeTest::RunTest(const FString& Parameters)
{
    FBreakerStatusApplicationSpec Spec;
    Spec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Void"));
    Spec.Duration = 4; Spec.TickInterval = 1; Spec.BaseDamagePerTick = 50;
    TestNull(TEXT("Retired Void has no runtime rule"), BreakerStatusRules::FindRule(Spec.StatusTag));    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    AActor* Target = World->SpawnActor<AActor>();
    AActor* Source = World->SpawnActor<AActor>();
    if (!Target || !Source) return false;
    UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Target);
    Target->AddInstanceComponent(ASC); ASC->RegisterComponent(); ASC->InitAbilityActorInfo(Target, Target);
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>(Target);
    ASC->AddAttributeSetSubobject(Attributes);
    Attributes->ApplyMaxHealth(1000); Attributes->ApplyHealth(500);
    ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetArmorAttribute(), 100);
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(Combat); Combat->RegisterComponent(); Combat->BindAttributes(Attributes);
    UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Target);
    Target->AddInstanceComponent(Status); Status->RegisterComponent();
    // A legacy serialized application is rejected, not converted into an inert
    // status or allowed to retain the removed armor/healing payload.
    for (int32 Attempt = 0; Attempt < 2; ++Attempt)
        Status->ApplyStatus(Spec, EBreakerDamageFamily::Elemental, Source);
    TestFalse(TEXT("Legacy Void application creates no status"), Status->HasStatus(Spec.StatusTag));
    TestEqual(TEXT("Retired Void changes no armor"), Combat->GetEffectiveArmor(), 100.0f);
    TestEqual(TEXT("Retired Void changes no healing"), Combat->ApplyHealingAmount(100, Source, FGameplayTag()).HealthHealed, 100.0f);
    const float Before = Attributes->GetHealth();
    Status->AdvanceStatuses(5);
    TestEqual(TEXT("Retired payload creates no delayed damage"), Attributes->GetHealth(), Before);
    TestEqual(TEXT("No inert placeholder occupies a status slot"), Status->GetActiveStatuses().Num(), 0);
    return true;
}
#endif