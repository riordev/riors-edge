#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerWardenEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerModifierComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerWardenFrontReviveRuntimeTest,"RiorsEdge.Combat.Warden.FrontReviveRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerWardenFrontReviveRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);if(!World)return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GFrameCounter=Frame;};
    auto* Warden=World->SpawnActor<ABreakerWardenEnemy>();if(!Warden)return false;
    Warden->ConfigureCrowdProbe();Warden->DispatchBeginPlay();Warden->SetActorTickEnabled(false);
    if(!TestTrue(TEXT("Native Wakeful modifier equips"),Warden->ConfigureWithExactModifiers({EBreakerEnemyModifier::Wakeful})))return false;
    auto* Combat=Warden->FindComponentByClass<UBreakerCombatComponent>();
    auto* Attributes=FindObject<UBreakerAttributeSet>(Warden,TEXT("Attributes"));
    if(!Combat||!Attributes)return false;
    if(!TestTrue(TEXT("Fresh Warden has authored front"),Combat->GetFrontShield()>0))return false;
    FBreakerDamageRequest Hit;Hit.BaseDamage=Combat->GetFrontShieldMax()*2;Hit.bCanCritical=false;Hit.bCanBeAvoided=false;
    Hit.DamageFamily=EBreakerDamageFamily::TrueDamage;Hit.bHasSourceLocation=true;
    Hit.SourceLocation=Warden->GetActorLocation()+Warden->GetActorForwardVector()*500;
    Combat->ReceiveDamage(Hit);
    if(!TestTrue(TEXT("Actual frontal hit breaks front"),Combat->IsFrontShieldBroken()))return false;
    Combat->RestoreVitals();
    TestTrue(TEXT("Direct body restore cannot rearm broken front"),Combat->IsFrontShieldBroken());
    Hit.bBypassShield=true;Hit.BaseDamage=Attributes->GetMaxHealth()*2;
    Combat->ReceiveDamage(Hit);
    TestTrue(TEXT("Lethal hit downs Wakeful body"),Combat->IsDead());
    const float Delay=Warden->GetModifierComponent()->GetWakefulReviveDelay();
    for(float T=0;T<Delay+.1f;T+=.01f){++GFrameCounter;World->Tick(LEVELTICK_All,.01f);}
    if(!TestTrue(TEXT("Actual Wakeful timer revives body"),IsValid(Warden)&&!Combat->IsDead()))return false;
    TestTrue(TEXT("Revived body has health"),Attributes->GetHealth()>0);
    TestTrue(TEXT("Revive preserves broken front"),Combat->IsFrontShieldBroken());
    TestEqual(TEXT("Revive grants no front pool"),Combat->GetFrontShield(),0.f);
    Hit.bBypassShield=false;Hit.BaseDamage=1;
    const float Before=Attributes->GetHealth();Combat->ReceiveDamage(Hit);
    TestTrue(TEXT("Frontal followup reaches revived body"),Attributes->GetHealth()<Before);
    return true;
}
#endif
