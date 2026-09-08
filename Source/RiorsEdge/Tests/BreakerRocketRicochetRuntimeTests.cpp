#include "Tests/BreakerRocketRicochetRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
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
#include "EngineUtils.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Weapons/BreakerRocketProjectile.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

void UBreakerRocketRicochetRuntimeObserver::OnHit(const FBreakerHitContext& Hit)
{ if(!Hit.bFromDoT) Hits.Add(Hit); }
void UBreakerRocketRicochetRuntimeObserver::OnExploded(const FVector& Location,float Radius)
{ ++Explosions; ExplosionLocation=Location; }

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRocketRicochetRuntimeTest,"RiorsEdge.Weapons.RocketRicochetRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerRocketRicochetRuntimeTest::RunTest(const FString&)
{
    for(int32 Scenario=0;Scenario<3;++Scenario)
    {
        UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
        if(!World) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
        const uint64 Frame=GFrameCounter;
        ON_SCOPE_EXIT {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GFrameCounter=Frame;};
        auto* Player=World->SpawnActor<ABreakerCharacter>(); if(!Player)return false;
        Player->SetActorTickEnabled(false);Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* Attr=Player->GetAttributes();auto* ASC=Player->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Player,Player);ASC->AddAttributeSetSubobject(Attr);Player->GetCombat()->BindAttributes(Attr);
        auto* Progression=Player->GetProgression();Progression->BindAttributes(Attr);
        auto* Class=NewObject<UBreakerClassDefinition>();Class->ClassId=EBreakerClassId::Caster;
        auto* Tree=NewObject<UBreakerProgressionTree>(Class);Tree->TreeId=TEXT("Test.Rocket.Ricochet");Tree->Currency=EBreakerPointCurrency::CorePoints;Class->BranchTrees.Add(Tree);
        auto* Node=NewObject<UBreakerProgressionNode>(Tree);Node->NodeId=TEXT("Test.Rocket.Ricochet.Rule");Node->Currency=Tree->Currency;
        FBreakerNodeEffect Effect;Effect.StatTarget=EBreakerNodeStatTarget::RicochetCount;Effect.ValuePerRank=1;Node->Effects.Add(Effect);Tree->Nodes.Add(Node);
        if(!Progression->ChoosePermanentClass(Class))return false;
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(2,Progression->ExperienceCurve));
        FText Reason;if(!TestTrue(TEXT("Ricochet uses an actual earned Core point"),Progression->PurchaseNode(Tree,Node->NodeId,Reason)))return false;
        FVector Eye;FRotator Aim;Player->GetActorEyesViewPoint(Eye,Aim);
        auto Wall=[&](FVector Location,FVector Extent)
        {
            auto* Actor=World->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Actor);
            Actor->AddInstanceComponent(Box);Actor->SetRootComponent(Box);Box->SetBoxExtent(Extent);
            Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionResponseToAllChannels(ECR_Block);
            Box->RegisterComponent();Actor->SetActorLocation(Location);return Actor;
        };
        Wall(Eye+FVector(400,0,0),FVector(20,200,200));
        auto* Enemy=World->SpawnActor<ABreakerEnemy>(Eye+(Scenario==2?FVector(100,2000,0):FVector(100,350,0)),FRotator::ZeroRotator);
        if(!Enemy)return false;
        Enemy->SetAreaLevel(100);Enemy->ConfigureCrowdProbe();Enemy->DispatchBeginPlay();Enemy->SetActorTickEnabled(false);
        if(auto* M=Enemy->FindComponentByClass<UBreakerEnemyMovementComponent>())M->SetComponentTickEnabled(false);
        if(auto* S=Enemy->FindComponentByClass<UBreakerStatusComponent>())S->SetComponentTickEnabled(false);
        auto* Combat=Enemy->FindComponentByClass<UBreakerCombatComponent>();Combat->SetComponentTickEnabled(false);
        auto* Observer=NewObject<UBreakerRocketRicochetRuntimeObserver>();
        Combat->OnDamageTaken.AddDynamic(Observer,&UBreakerRocketRicochetRuntimeObserver::OnHit);
        auto* Weapon=Player->GetWeapon();Weapon->WeaponDefinition=nullptr;Weapon->SetSlotArchetype(1,EBreakerWeaponArchetype::Rocket);Weapon->ResetAmmunition();
        auto Tick=[&](){++GFrameCounter;World->Tick(LEVELTICK_All,.005f);};
        for(int32 I=0;I<400;++I)Tick();
        const float Base=Weapon->GetScaledBaseDamage()*Attr->GetDamageMultiplier(),Crit=Attr->GetCriticalMultiplier();
        const int32 Ammo=Weapon->GetMagazineAmmo();Weapon->StartFire();Weapon->StopFire();
        TestEqual(TEXT("Native rocket emission debits exactly one round"),Weapon->GetMagazineAmmo(),Ammo-1);
        ABreakerRocketProjectile* Rocket=nullptr;
        for(TActorIterator<ABreakerRocketProjectile> It(World);It;++It)if(It->GetOwner()==Player&&!It->HasExploded())Rocket=*It;
        if(!TestNotNull(TEXT("Native weapon spawned the projectile"),Rocket))return false;
        if(!Rocket->HasActorBegunPlay())Rocket->DispatchBeginPlay();
        Rocket->RegisterAllActorTickFunctions(true,true);
        Rocket->OnExploded.AddDynamic(Observer,&UBreakerRocketRicochetRuntimeObserver::OnExploded);
        const float OriginalLifetime=Rocket->GetLifeSpan();
        if(!TestTrue(TEXT("Native flight has a finite lifetime"),OriginalLifetime>0))return false;
        if(!Progression->RespecCore(Reason))return false;
        int32 Frames=0;
        while(IsValid(Rocket)&&!Rocket->HasExploded()&&!Rocket->HasRicocheted()&&Frames<400){Tick();++Frames;}
        if(Scenario==2)
        {
            TestFalse(TEXT("Out-of-seek enemy cannot grant a redirect"),Rocket->HasRicocheted());
            TestTrue(TEXT("Unsuccessful seek preserves terminal world explosion"),Rocket->HasExploded());
            TestEqual(TEXT("World fallback explodes exactly once"),Observer->Explosions,1);
            TestTrue(TEXT("Out-of-range victim receives no blast"),Observer->Hits.IsEmpty());
            continue;
        }
        if(!TestTrue(TEXT("Actual wall collision performs earned seeking redirect"),Rocket->HasRicocheted()))
        {
            AddError(FString::Printf(TEXT("Flight stopped at %s after %d frames; exploded=%d"), *Rocket->GetActorLocation().ToString(), Frames, Rocket->HasExploded()));
            return false;
        }
        TestEqual(TEXT("Successful world redirect has not exploded"),Observer->Explosions,0);
        TestTrue(TEXT("Redirect does not reset flight lifetime"),Rocket->GetLifeSpan()<=OriginalLifetime-Frames*.005f+.006f);
        if(Scenario==1)
            Wall((Rocket->GetActorLocation()+Enemy->GetActorLocation())*.5f,FVector(45,45,100));
        while(IsValid(Rocket)&&!Rocket->HasExploded()&&Frames<800){Tick();++Frames;}
        if(!TestTrue(TEXT("Real world ticks reach a terminal impact"),IsValid(Rocket)&&Rocket->HasExploded()))return false;
        TestEqual(TEXT("Same redirected actor emits one final blast"),Observer->Explosions,1);
        if(Scenario==1)
        {
            TestTrue(TEXT("Second world collision terminates instead of another seek"),FVector::Dist(Observer->ExplosionLocation,Enemy->GetActorLocation())>100);
            continue;
        }
        if(!TestEqual(TEXT("Terminal seeker blast reaches actual native enemy once"),Observer->Hits.Num(),1))return false;
        const auto& Hit=Observer->Hits[0];
        const float Distance=FVector::Dist(Enemy->GetActorLocation(),Observer->ExplosionLocation);
        const float Falloff=FMath::Lerp(1.f,Rocket->EdgeDamageFraction,FMath::Clamp(Distance/Weapon->GetActiveDefinition()->ExplosionRadius,0.f,1.f));
        TestEqual(TEXT("Retained emitted source pays one ricochet attenuation and normal blast falloff"),Hit.Result.RawDamage,
            Base*Weapon->RicochetDamageMultiplier*Falloff*(Hit.Result.bCritical?Crit:1.f),.03f);
        TestEqual(TEXT("Ricochet limits proc strength to one half"),Hit.ProcCoefficient,.5f,.001f);
        TestTrue(TEXT("Real native impact credits original weapon owner"),Hit.Instigator==Player);
    }
    return true;
}
#endif
