#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerChargeComponent.h"
#include "Classes/BreakerGritComponent.h"
#include "Classes/BreakerManaComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Classes/BreakerScrapComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Classes/BreakerResourceGeneration.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Combat/BreakerDeployable.h"
#include "Tests/BreakerThreatRuntimeObserver.h"
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::ThreatGenerated)==63);
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::DeployableHealth)==64);
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::HealingReceived)==65);
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::HealthRegenPercentMaxHealth)==66);
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::ShieldRechargeDelayReduction)==67);
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerThreatNodesRuntimeTest,"RiorsEdge.Combat.Threat.CorePrimitives",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerThreatNodesRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!World)return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
    World->InitializeActorsForPlay(FURL());
    auto PlayerAt=[&](FVector Location)
    {
        auto* P=World->SpawnActor<ABreakerCharacter>(Location,FRotator::ZeroRotator);
        P->GetAbilitySystemComponent()->InitAbilityActorInfo(P,P);
        P->GetAbilitySystemComponent()->AddAttributeSetSubobject(P->GetAttributes());
        P->GetCombat()->BindAttributes(P->GetAttributes()); P->GetProgression()->BindAttributes(P->GetAttributes());
        return P;
    };
    auto* Near=PlayerAt(FVector(500,0,100)); auto* Far=PlayerAt(FVector(900,0,100));
    auto* P=Far->GetProgression();
    auto* Tree=NewObject<UBreakerProgressionTree>(); Tree->TreeId=TEXT("Test.Threat.Core"); Tree->Currency=EBreakerPointCurrency::CorePoints;
    auto* Node=NewObject<UBreakerProgressionNode>(Tree); Node->NodeId=TEXT("Test.Threat.Core.Rank"); Node->Currency=Tree->Currency;
    for(auto Target:{EBreakerNodeStatTarget::ThreatGenerated,EBreakerNodeStatTarget::DeployableHealth})
    {FBreakerNodeEffect E; E.StatTarget=Target; E.StatBucket=EBreakerNodeStatBucket::IncreasedPercent; E.ValuePerRank=50; Node->Effects.Add(E);}
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Provocation"))); Tree->Nodes.Add(Node);
    auto* Class=NewObject<UBreakerClassDefinition>(); Class->ClassId=EBreakerClassId::Gunsmith; Class->BranchTrees.Add(Tree);
    if(!P->ChoosePermanentClass(Class))return false;
    P->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(2,P->ExperienceCurve));
    auto* Enemy=World->SpawnActor<ABreakerEnemy>(FVector(0,0,100),FRotator::ZeroRotator);
    Enemy->ConfigureCrowdProbe(); Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
    auto* Victim=Enemy->FindComponentByClass<UBreakerCombatComponent>();
    auto Hit=[&](AActor* RewardOwner,AActor* Direct,float Amount)
    {
        FBreakerDamageRequest R; R.BaseDamage=Amount; R.DamageFamily=EBreakerDamageFamily::TrueDamage;
        R.bCanCritical=false; R.bCanBeAvoided=false; R.SetInstigator(RewardOwner); R.ThreatSource=Direct;
        return Victim->ReceiveDamage(R);
    };
    auto Select=[&](){Enemy->Tick(0);return Enemy->GetThreatTarget();};
    Hit(Near,nullptr,12); Hit(Far,nullptr,10);
    TestTrue(TEXT("Unowned weaker damage retains higher-threat near player"),Select()==Near);
    auto Deploy=[&]()
    {
        auto* D=World->SpawnActor<ABreakerDeployable>(FVector(700,200,100),FRotator::ZeroRotator);
        D->DispatchBeginPlay(); D->SetActorTickEnabled(false); D->InitializeDeployable(EBreakerDeployableType::Turret,Far,0);
        return D;
    };
    auto* Base=Deploy(); const float BaseHealth=Base->FindComponentByClass<UBreakerCombatComponent>()->GetMaxHealth();
    FText Reason; if(!TestTrue(TEXT("Earned schema point buys actual effects"),P->PurchaseNode(Tree,Node->NodeId,Reason)))return false;
    Hit(Near,nullptr,12); Hit(Far,nullptr,10);
    TestTrue(TEXT("Increased applies once to new accepted credit:25 beats24"),Select()==Far);
    auto* Strong=Deploy(); auto* StrongCombat=Strong->FindComponentByClass<UBreakerCombatComponent>();
    TestEqual(TEXT("New deployment scales authored chassis once"),StrongCombat->GetMaxHealth(),BaseHealth*1.5f,.001f);
    TestEqual(TEXT("Existing deployment does not gain free health"),Base->FindComponentByClass<UBreakerCombatComponent>()->GetMaxHealth(),BaseHealth,.001f);
    auto* Observer=NewObject<UBreakerThreatRuntimeObserver>(Far); Far->GetCombat()->OnHitDealt.AddDynamic(Observer,&UBreakerThreatRuntimeObserver::Observe);
    Hit(Far,Strong,20);
    TestTrue(TEXT("Producer gets owner-scaled30 threat and beats player25"),Select()==Strong);
    TestTrue(TEXT("Reward owner stays player"),Observer->LastRewardOwner==Far);
    TestTrue(TEXT("Threat remains direct deployable"),Observer->LastThreatSource==Strong);
    auto* Ranged=World->SpawnActor<ABreakerRangedEnemy>(FVector(0,1000,100),FRotator::ZeroRotator);
    Ranged->DispatchBeginPlay(); Ranged->SetActorTickEnabled(false);
    FBreakerDamageRequest Pull; Pull.BaseDamage=1; Pull.DamageFamily=EBreakerDamageFamily::TrueDamage;
    Pull.bCanCritical=false; Pull.bCanBeAvoided=false; Pull.SetInstigator(Far);
    Ranged->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Pull);
    Ranged->Tick(.05f);
    TestTrue(TEXT("Actual ranged windup commits protected target"),Ranged->GetCommittedAttackTarget()==Far);
    TestEqual(TEXT("Reduced accuracy divides existing aim-error lane by .9"),Ranged->GetComposedAimErrorMultiplier(),1.f/.9f,.001f);
    if(!P->RespecCore(Reason))return false;
    TestEqual(TEXT("Respec removes live targeted aim error"),Ranged->GetComposedAimErrorMultiplier(),1.f,.001f);
    TestEqual(TEXT("Respec never heals or rewrites deployed snapshot"),StrongCombat->GetMaxHealth(),BaseHealth*1.5f,.001f);
    auto* After=Deploy(); TestEqual(TEXT("New deployment after respec has ordinary health"),After->FindComponentByClass<UBreakerCombatComponent>()->GetMaxHealth(),BaseHealth,.001f);
    return true;
}
#endif