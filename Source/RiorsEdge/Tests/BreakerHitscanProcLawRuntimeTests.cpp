#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/BreakerCoreLoudRuntimeObserver.h"
#include "AbilitySystemComponent.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerHitscanProcLawRuntimeTest,"RiorsEdge.Combat.ProcCoefficient.Law",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerHitscanProcLawRuntimeTest::RunTest(const FString&)
{
    UWorld::InitializationValues Init;Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);if(!World)return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GFrameCounter=Frame;};
    auto* Player=World->SpawnActor<ABreakerCharacter>();if(!Player)return false;
    Player->SetActorTickEnabled(false);Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* Attr=Player->GetAttributes();auto* ASC=Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player,Player);ASC->AddAttributeSetSubobject(Attr);Player->GetCombat()->BindAttributes(Attr);
    auto* Progression=Player->GetProgression();Progression->BindAttributes(Attr);
    auto* Class=NewObject<UBreakerClassDefinition>();Class->ClassId=EBreakerClassId::Caster;
    auto* Tree=NewObject<UBreakerProgressionTree>(Class);Tree->TreeId=TEXT("Test.NativeProc");Tree->Currency=EBreakerPointCurrency::CorePoints;Class->BranchTrees.Add(Tree);
    auto Node=[&](const TCHAR* Id,EBreakerNodeStatTarget Stat)
    {auto* N=NewObject<UBreakerProgressionNode>(Tree);N->NodeId=Id;N->Currency=Tree->Currency;FBreakerNodeEffect E;E.StatTarget=Stat;E.ValuePerRank=1;N->Effects.Add(E);Tree->Nodes.Add(N);return N;};
    auto* Ricochet=Node(TEXT("Test.NativeProc.Ricochet"),EBreakerNodeStatTarget::RicochetCount);
    auto* Chain=Node(TEXT("Test.NativeProc.Chain"),EBreakerNodeStatTarget::ChainCount);
    auto* Fan=NewObject<UBreakerProgressionNode>(Tree);Fan->NodeId=TEXT("Test.NativeProc.Fan");Fan->Currency=Tree->Currency;
    // Isolated delivery primitive, not proof of acquiring the live replacement
    // wheel: retain Fan's authored five-point price and earn the whole wallet.
    Fan->CostPerRank=5;
    Fan->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Fan")));Tree->Nodes.Add(Fan);
    if(!Progression->ChoosePermanentClass(Class))return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(7,Progression->ExperienceCurve));
    if(!TestEqual(TEXT("Earned wallet pays Fan five plus both one-point channels"),Progression->GetUnspentPoints(Tree->Currency),7))return false;
    FText Reason;
    if(!Progression->PurchaseNode(Tree,Ricochet->NodeId,Reason)||!Progression->PurchaseNode(Tree,Chain->NodeId,Reason))return false;
    auto* Observer=NewObject<UBreakerCoreLoudRuntimeObserver>();
    FVector Eye;FRotator Aim;Player->GetActorEyesViewPoint(Eye,Aim);
    auto Spawn=[&](FVector Location)
    {
        auto* E=World->SpawnActor<ABreakerEnemy>(Location,FRotator::ZeroRotator);if(!E)return E;
        E->SetAreaLevel(100);E->ConfigureCrowdProbe();E->DispatchBeginPlay();E->SetActorTickEnabled(false);
        if(auto* M=E->FindComponentByClass<UBreakerEnemyMovementComponent>())M->SetComponentTickEnabled(false);
        if(auto* S=E->FindComponentByClass<UBreakerStatusComponent>())S->SetComponentTickEnabled(false);
        auto* C=E->FindComponentByClass<UBreakerCombatComponent>();C->SetComponentTickEnabled(false);
        C->OnDamageTaken.AddDynamic(Observer,&UBreakerCoreLoudRuntimeObserver::OnHit);return E;
    };
    auto* A=Spawn(Eye+FVector(100,350,0));auto* B=Spawn(Eye+FVector(100,600,0));if(!A||!B)return false;
    auto* Wall=World->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Wall);Wall->AddInstanceComponent(Box);Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(20,200,200));Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->RegisterComponent();Wall->SetActorLocation(Eye+FVector(400,0,0));
    auto* Weapon=Player->GetWeapon();Weapon->ResetAmmunition();
    auto Clock=[&](float Seconds){for(int32 I=0;I<FMath::CeilToInt(Seconds*100);++I){++GFrameCounter;World->Tick(LEVELTICK_All,.01f);}};
    auto Fire=[&]()
    {
        if(Weapon->GetMagazineAmmo()==0){Weapon->StartReload();Clock(5);}
        Clock(.5f);Observer->Hits.Reset();
        const int32 Before=Weapon->GetMagazineAmmo();Weapon->StartFire();Weapon->StopFire();
        return TestEqual(TEXT("Actual native shot pays ammunition"),Weapon->GetMagazineAmmo(),Before-1);
    };
    if(!Fire())return false;
    AddInfo(FString::Printf(TEXT("First hit=%s legs=%d ricochet=%d A=%s"),*GetNameSafe(Weapon->GetLastShot().HitActor.Get()),Weapon->GetLastShot().SecondaryImpacts.Num(),Weapon->GetShotChannels().RicochetCount,*A->GetActorLocation().ToString()));
    if(!TestEqual(TEXT("Ricochet cannot generate the separately owned chain"),Observer->Hits.Num(),1))return false;
    TestTrue(TEXT("Nearest visible enemy receives ricochet"),Observer->Hits[0].Target==A);
    TestEqual(TEXT("Native ricochet submits half proc"),Observer->Hits[0].ProcCoefficient,.5f,.001f);
    TestEqual(TEXT("Ricochet damage retains independent authored attenuation"),Observer->Hits[0].Result.RawDamage,
        Weapon->GetScaledBaseDamage()*Attr->GetDamageMultiplier()*Weapon->RicochetDamageMultiplier
        *(Observer->Hits[0].Result.bCritical?Attr->GetCriticalMultiplier():1.f),.03f);
    Wall->SetActorEnableCollision(false);A->SetActorLocation(Eye+FVector(150,0,0));B->SetActorLocation(Eye+FVector(150,220,0));
    if(!Fire()||!TestEqual(TEXT("Straight native control still uses owned chain"),Observer->Hits.Num(),2))return false;
    for(const auto& Hit:Observer->Hits)TestEqual(TEXT("Ordinary straight and chain hits retain original proc"),Hit.ProcCoefficient,1.f,.001f);
    Wall->SetActorEnableCollision(true);A->SetActorLocation(Eye+FVector(100,350,0));B->SetActorLocation(Eye+FVector(100,600,0));
    // The native rifle has no authored bleed. Use shipped SMG's real chance
    // and starting ammunition; do not alter a definition or fabricate status.
    Weapon->WeaponDefinition=nullptr;Weapon->SetSlotArchetype(1,EBreakerWeaponArchetype::SMG);Weapon->ResetAmmunition();
    auto* Status=A->FindComponentByClass<UBreakerStatusComponent>();
    bool bApplied=false;
    for(int32 Shot=0;Shot<200&&!bApplied;++Shot)
    {
        if(!Fire())return false;
        for(const auto& Active:Status->GetActiveStatuses())
            if(Active.Spec.StatusTag==FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed")))
            {bApplied=true;TestEqual(TEXT("Actual weapon-authored bleed carries half proc"),Active.Spec.ProcCoefficient,.5f,.001f);}
    }
    if(!TestTrue(TEXT("Native SMG delivered real bleed within its actual ammunition supply"),bApplied))return false;
    if(!Progression->PurchaseNode(Tree,Fan->NodeId,Reason)||!Fire())return false;
    if(!TestEqual(TEXT("Paid Fan emits three native seeking pellets"),Observer->Hits.Num(),3))return false;
    int32 Half=0,Zero=0;
    for(const auto& Hit:Observer->Hits)
    {
        if(FMath::IsNearlyEqual(Hit.ProcCoefficient,.5f))++Half;
        if(FMath::IsNearlyZero(Hit.ProcCoefficient))++Zero;
        TestTrue(TEXT("Extra pellet damage remains real"),Hit.Result.RawDamage>0);
    }
    TestEqual(TEXT("Only original pellet retains half proc"),Half,1);
    TestEqual(TEXT("Fan copies stay zero through ricochet"),Zero,2);
    return true;
}
#endif
