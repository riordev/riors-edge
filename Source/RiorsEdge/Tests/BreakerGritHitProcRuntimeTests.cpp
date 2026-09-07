#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerGritComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerGritHitProcRuntimeTest, "RiorsEdge.Classes.Grit.HitProcRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerGritHitProcRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Attacker = World->SpawnActor<AActor>();
    if (!Attacker) return false;
    for (float Proc : {0.0f, .25f, 1.0f})
    {
        auto* Player = World->SpawnActor<ABreakerCharacter>();
        if (!Player) return false;
        auto* ASC = Player->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Player, Player);
        ASC->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->BindAttributes(Player->GetAttributes());
        if (!TestTrue(TEXT("ordinary Tank selection"), Player->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Tank))) return false;
        auto* Grit = Player->GetGrit();
        Grit->BindAttributes(Player->GetAttributes());
        Grit->SetInCombat(true);
        Grit->AdvanceLoop(1);
        // Native Character callback without Character BeginPlay/save loading.
        FScriptDelegate Delegate;
        Delegate.BindUFunction(Player, TEXT("HandleClassResourceDamageTaken"));
        Player->GetCombat()->OnDamageTaken.Add(Delegate);
        const float BeforeGrit = Grit->GetGrit();
        const float MaxHealth = Player->GetAttributes()->GetMaxHealth();
        FBreakerDamageRequest Hit;
        Hit.BaseDamage = MaxHealth * .1f;
        Hit.DamageFamily = EBreakerDamageFamily::Elemental;
        Hit.bIsDamageOverTime = true;
        Hit.bCanApplyElementBuildup = false;
        Hit.bCanCritical = false;
        Hit.ProcCoefficient = Proc;
        Hit.SetInstigator(Attacker);
        const auto Result = Player->GetCombat()->ReceiveDamage(Hit);
        TestEqual(TEXT("proc changes generation, not received damage"), Result.HealthDamage, MaxHealth * .1f, .001f);
        Grit->AdvanceLoop(1);
        TestEqual(TEXT("real hit callback pays proportional capped Grit"), Grit->GetGrit() - BeforeGrit, 5.0f * Proc, .001f);
        Player->Destroy();
    }
    return true;
}
#endif
