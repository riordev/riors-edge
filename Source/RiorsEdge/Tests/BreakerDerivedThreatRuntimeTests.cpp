#include "Tests/BreakerThreatRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDeployable.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerDerivedThreatRuntimeTest,"RiorsEdge.Combat.Threat.DerivedAttribution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerDerivedThreatRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto* Owner=World->SpawnActor<ABreakerCharacter>(FVector(1000,0,100),FRotator::ZeroRotator);
    auto* Enemy=World->SpawnActor<ABreakerEnemy>(FVector(0,0,100),FRotator::ZeroRotator);
    if (!Owner || !Enemy) return false;
    Owner->GetAbilitySystemComponent()->InitAbilityActorInfo(Owner,Owner);
    Owner->GetAbilitySystemComponent()->AddAttributeSetSubobject(Owner->GetAttributes());
    Owner->GetCombat()->BindAttributes(Owner->GetAttributes());
    Enemy->ConfigureCrowdProbe(); Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
    auto* Combat=Enemy->FindComponentByClass<UBreakerCombatComponent>();
    auto* Status=Enemy->FindComponentByClass<UBreakerStatusComponent>();
    if (!Combat || !Status) return false;
    auto* Observer=NewObject<UBreakerThreatRuntimeObserver>(Owner);
    Owner->GetCombat()->OnHitDealt.AddDynamic(Observer,&UBreakerThreatRuntimeObserver::Observe);
    auto SpawnProducer=[&]()
    {
        auto* D=World->SpawnActor<ABreakerDeployable>(FVector(700,200,100),FRotator::ZeroRotator);
        if (D) { D->DispatchBeginPlay(); D->InitializeDeployable(EBreakerDeployableType::Turret,Owner,0); D->SetActorTickEnabled(false); }
        return D;
    };
    auto* Producer=SpawnProducer();
    if (!Producer) return false;
    auto Hit=[&](EBreakerElement Element,float Amount,AActor* Direct)
    {
        FBreakerDamageRequest Request; Request.BaseDamage=Amount; Request.Element=Element; Request.ElementalFraction=1;
        Request.DamageFamily=EBreakerDamageFamily::Elemental; Request.bCanCritical=false; Request.bCanBeAvoided=false;
        Request.SetInstigator(Owner); Request.ThreatSource=Direct;
        return Combat->ReceiveDamage(Request);
    };
    auto Reset=[&]() { Status->ConsumeAllStatuses(); Status->AdvanceStatuses(30); Combat->RestoreVitals(); Observer->Hits=0; };
    for (EBreakerElement Element : {EBreakerElement::Entropy,EBreakerElement::Void,EBreakerElement::Rift})
    {
        Reset();
        const float Threshold=Element==EBreakerElement::Entropy ? Status->GetEntropyThreshold()
            : Element==EBreakerElement::Void ? Status->GetVoidThreshold() : Status->GetRiftThreshold();
        // Shipped chassis and resistance remain intact; four threshold units
        // cross the supported resistance while leaving the native body alive.
        Hit(Element,Threshold*4,Producer);
        if (Element!=EBreakerElement::Rift)
        {
            if (!TestTrue(TEXT("Actual applying hit earns elemental status"),!Status->GetActiveStatuses().IsEmpty())) return false;
            const auto Active=Status->GetActiveStatuses()[0];
            TestTrue(TEXT("Application snapshots direct producer"),Active.bHasThreatSource && Active.ThreatSource.Get()==Producer);
            Observer->Hits=0;
            Status->AdvanceStatuses(Element==EBreakerElement::Entropy ? Active.Spec.TickInterval+.01f : Active.Spec.Duration+.01f);
            TestTrue(TEXT("Actual derived payout occurs"),Observer->Hits>0);
        }
        else TestTrue(TEXT("Actual Rift direct hit and earned burst both occurred"),Observer->Hits>=2);
        TestTrue(TEXT("Derived payout credits original reward owner"),Observer->LastRewardOwner==Owner);
        TestTrue(TEXT("Derived payout retains direct producer"),Observer->LastThreatSource==Producer);
    }
    Reset();
    Hit(EBreakerElement::Entropy,Status->GetEntropyThreshold()*4,Producer);
    if (!TestTrue(TEXT("Rot earned before reaction"),Status->HasStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"))))) return false;
    auto* ReactingProducer=SpawnProducer();
    if (!ReactingProducer) return false;
    Observer->Hits=0;
    Hit(EBreakerElement::Void,1,ReactingProducer);
    TestTrue(TEXT("Reaction pays after triggering direct hit"),Observer->Hits>=2);
    TestTrue(TEXT("Reaction spends original producer budget rather than trigger producer"),Observer->LastThreatSource==Producer);
    TestTrue(TEXT("Reaction reward owner preserved"),Observer->LastRewardOwner==Owner);
    Reset();
    FBreakerStatusApplicationSpec Bleed;
    Bleed.StatusTag=FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed")); Bleed.Duration=3; Bleed.TickInterval=1; Bleed.BaseDamagePerTick=5;
    FBreakerDamageRequest Applying; Applying.SetInstigator(Owner); Applying.ThreatSource=Producer;
    Status->ApplyStatusFromHit(Bleed,EBreakerDamageFamily::Physical,Applying);
    Status->AdvanceStatuses(1.01f);
    TestTrue(TEXT("Physical periodic status retains producer"),Observer->LastThreatSource==Producer);
    Applying.ThreatSource=ReactingProducer;
    Status->ApplyStatusFromHit(Bleed,EBreakerDamageFamily::Physical,Applying);
    Status->AdvanceStatuses(1.01f);
    TestTrue(TEXT("Refresh follows existing latest-applier attribution policy"),Observer->LastThreatSource==ReactingProducer);
    ReactingProducer->Destroy(); Observer->Hits=0;
    Status->AdvanceStatuses(1.01f);
    TestTrue(TEXT("Status outlives destroyed producer and still pays"),Observer->Hits>0);
    TestTrue(TEXT("Destroyed producer does not become owner threat"),Observer->LastThreatSource==nullptr);
    TestTrue(TEXT("Destroyed producer does not lose owner reward credit"),Observer->LastRewardOwner==Owner);
    return true;
}
#endif
