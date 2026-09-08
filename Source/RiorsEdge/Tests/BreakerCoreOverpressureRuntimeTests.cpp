#include "Tests/BreakerCoreOverpressureRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Weapons/BreakerRocketProjectile.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

void UBreakerCoreOverpressureRuntimeObserver::OnHit(const FBreakerHitContext& Hit)
{
    if (Hit.bFromDoT) return;
    Hits.Add(Hit);
    if (bRevokeOnParent && !Hit.bFundedWeaponSplash)
    {
        bRevokeOnParent=false;
        FText Reason;
        bRespecSucceeded=Progression && Progression->RespecCore(Reason);
        if (bKillSource && SourceCombat)
        {
            FBreakerDamageRequest Lethal; Lethal.BaseDamage=SourceCombat->GetMaxHealth()*2;
            Lethal.DamageFamily=EBreakerDamageFamily::TrueDamage; Lethal.bCanCritical=false; Lethal.bCanBeAvoided=false;
            SourceCombat->ReceiveDamage(Lethal);
        }
    }
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreOverpressureRuntimeTest,"RiorsEdge.Weapons.CoreOverpressureRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreOverpressureRuntimeTest::RunTest(const FString&)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter=Frame; };
    auto* Player=World->SpawnActor<ABreakerCharacter>(); if (!Player) return false;
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* Attr=Player->GetAttributes(); auto* ASC=Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attr); Player->GetCombat()->BindAttributes(Attr);
    auto* Progression=Player->GetProgression(); Progression->BindAttributes(Attr);
    auto* Class=NewObject<UBreakerClassDefinition>(); Class->ClassId=EBreakerClassId::Caster;
    auto* Tree=NewObject<UBreakerProgressionTree>(Class); Tree->TreeId=TEXT("Test.Overpressure"); Tree->Currency=EBreakerPointCurrency::CorePoints; Class->BranchTrees.Add(Tree);
    auto Make=[&](const TCHAR* Id,const TCHAR* Tag)
    { auto* N=NewObject<UBreakerProgressionNode>(Tree); N->NodeId=Id; N->Currency=Tree->Currency; N->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(Tag)); Tree->Nodes.Add(N); return N; };
    auto* Rule=Make(TEXT("Test.Overpressure.Rule"),TEXT("Progression.Node.Core.Ballistics.Overpressure"));
    auto* Loud=Make(TEXT("Test.Overpressure.Loud"),TEXT("Progression.Node.Core.Ballistics.Loud"));
    auto* Area=NewObject<UBreakerProgressionNode>(Tree); Area->NodeId=TEXT("Test.Overpressure.Area"); Area->Currency=Tree->Currency;
    FBreakerNodeEffect AreaEffect; AreaEffect.StatTarget=EBreakerNodeStatTarget::WeaponSplashArea;
    Area->MaxRank=3; Area->CostPerRank=1;
    AreaEffect.StatBucket=EBreakerNodeStatBucket::IncreasedPercent; AreaEffect.ValuePerRank=6;
    Area->Effects.Add(AreaEffect); Tree->Nodes.Add(Area);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5,Progression->ExperienceCurve));
    auto* Weapon=Player->GetWeapon();
    if(!TestEqual(TEXT("Native fixture starts on Primary slot one"),Weapon->GetCurrentSlot(),1)) return false;
    const float Range=Weapon->GetEffectiveMaximumRange();
    FText Reason; if (!Progression->PurchaseNode(Tree,Rule->NodeId,Reason)||!Progression->PurchaseNode(Tree,Loud->NodeId,Reason)) return false;
    TestEqual(TEXT("Forfeit halves actual maximum range"),Weapon->GetEffectiveMaximumRange(),Range*.5f,.001f);
    auto* Observer=NewObject<UBreakerCoreOverpressureRuntimeObserver>();
    Observer->Progression=Progression; Observer->SourceCombat=Player->GetCombat();
    auto Spawn=[&](FVector P)
    {
        auto* E=World->SpawnActor<ABreakerEnemy>(P,FRotator::ZeroRotator);
        if (!E) return E;
        E->SetAreaLevel(100); E->ConfigureCrowdProbe(); E->DispatchBeginPlay(); E->SetActorTickEnabled(false);
        if(auto* M=E->FindComponentByClass<UBreakerEnemyMovementComponent>()) M->SetComponentTickEnabled(false);
        if(auto* S=E->FindComponentByClass<UBreakerStatusComponent>()) S->SetComponentTickEnabled(false);
        auto* C=E->FindComponentByClass<UBreakerCombatComponent>(); C->SetComponentTickEnabled(false);
        C->OnDamageTaken.AddDynamic(Observer,&UBreakerCoreOverpressureRuntimeObserver::OnHit); return E;
    };
    FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye,Aim);
    auto* A=Spawn(Eye+FVector(150,0,0)); auto* B=Spawn(Eye+FVector(150,150,0));
    auto* C=Spawn(Eye+FVector(150,450,0)); auto* D=Spawn(Eye+FVector(150,900,0));
    if(!A||!B||!C||!D) return false;
    auto Clock=[&](float Seconds){for(int32 I=0;I<FMath::CeilToInt(Seconds*100);++I){++GFrameCounter;World->Tick(LEVELTICK_All,.01f);}};
    auto Fire=[&]()
    {
        if(Weapon->GetMagazineAmmo()==0){Weapon->StartReload();Clock(5);}
        Clock(FMath::Max(1.f,60.f/Weapon->GetEffectiveRoundsPerMinute(Weapon->GetActiveDefinition())+.05f));
        const int32 Ammo=Weapon->GetMagazineAmmo();Weapon->StartFire();Weapon->StopFire();
        TestEqual(TEXT("Native trigger spends one actual round"),Weapon->GetMagazineAmmo(),Ammo-1);
    };
    Weapon->ResetAmmunition(); Fire();
    if(!TestEqual(TEXT("Rifle hit plus ordinary splash and one Loud extension"),Observer->Hits.Num(),3)) return false;
    const float ParentRaw=Observer->Hits[0].Result.RawDamage;
    TestTrue(TEXT("Original rifle target stays first"),Observer->Hits[0].Target==A);
    for(int32 I=1;I<Observer->Hits.Num();++I)
    {
        const auto& Hit=Observer->Hits[I];
        TestTrue(TEXT("Only nearby and Loud extra receive children"),Hit.Target==B||Hit.Target==C);
        TestTrue(TEXT("Children explicitly identify funded splash"),Hit.bFundedWeaponSplash);
        TestFalse(TEXT("Children are direct splash, never fake DoT"),Hit.bFromDoT);
        TestFalse(TEXT("Earned critical cannot be rolled twice"),Hit.Result.bCritical);
        TestEqual(TEXT("Children retain exact forty percent parent raw"),Hit.Result.RawDamage,ParentRaw*.4f,.002f);
        TestEqual(TEXT("Children cannot generate new procs"),Hit.ProcCoefficient,0.f);
    }
    auto* ACombat=A->FindComponentByClass<UBreakerCombatComponent>();
    ACombat->ArmFrontShield(100); A->SetActorRotation((Player->GetActorLocation()-A->GetActorLocation()).Rotation());
    const float AHealthBefore=A->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>()->GetHealth();
    Observer->Hits.Reset(); Fire();
    if(!TestEqual(TEXT("Front-only accepted rifle hit still funds both children"),Observer->Hits.Num(),3)) return false;
    TestEqual(TEXT("Archetype front absorbs without health loss"),A->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>()->GetHealth(),AHealthBefore,.002f);
    TestTrue(TEXT("Parent result reports front pool expenditure"),Observer->Hits[0].Result.ShieldDamage>0);
    TestTrue(TEXT("Native front pool actually spent"),ACombat->GetFrontShield()<100);
    ACombat->ArmFrontShield(0);
    // Resolve a real weapon source with explicit mixed conversion to exercise
    // the production allocation boundary independently of random loot rolls.
    FBreakerDamageRequest Mixed; Mixed.BaseDamage=20; Mixed.SetInstigator(Player); Mixed.bCanCritical=false; Mixed.bCanBeAvoided=false;
    UBreakerDamageLibrary::FillSourcePools(Attr,EBreakerDamageDelivery::Weapon,Mixed);
    FBreakerElementShare Entropy; Entropy.Element=EBreakerElement::Entropy; Entropy.Fraction=.4f; Mixed.ElementShares.Add(Entropy);
    Mixed.ElementSource.ElementalDamageIncreasedPercent=50;
    Mixed.bHasImpactLocation=true; Mixed.ImpactLocation=A->GetActorLocation();
    auto* BCombat=B->FindComponentByClass<UBreakerCombatComponent>();
    auto* BAttr=const_cast<UBreakerAttributeSet*>(B->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>());
    BAttr->SetAggregatedAttributeBase(EBreakerAggregatedAttribute::Armor,100);
    BAttr->SetEquipmentShieldCapacity(1); BAttr->ApplyShield(1);
    BCombat->ArmFrontShield(1); B->SetActorRotation((A->GetActorLocation()-B->GetActorLocation()).Rotation());
    const float BHealthBefore=BAttr->GetHealth();
    Observer->Hits.Reset(); A->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Mixed);
    if(!TestEqual(TEXT("Mixed source still creates only two children"),Observer->Hits.Num(),3)) return false;
    const auto Parent=Observer->Hits[0].Result;
    const float BMitigated=Parent.RawDamage*.4f*(1-UBreakerDamageLibrary::CalculateArmorMitigation(100,0));
    TestTrue(TEXT("Real secondary payout exceeds both deliberately small shield layers"),BMitigated>2);
    TestEqual(TEXT("Secondary front pool absorbs first"),BCombat->GetFrontShield(),0.f,.002f);
    TestEqual(TEXT("Secondary ward absorbs next"),BAttr->GetShield(),0.f,.002f);
    TestEqual(TEXT("Only secondary-armour-mitigated remainder reaches health"),BAttr->GetHealth(),BHealthBefore-(BMitigated-2),.02f);
    for(int32 I=1;I<3;++I)
    {
        const auto& Child=Observer->Hits[I].Result;
        TestEqual(TEXT("Funded physical amount preserved"),Child.UnconvertedRawDamage,Parent.UnconvertedRawDamage*.4f,.002f);
        if(!TestEqual(TEXT("Funded conversion retains one typed part"),Child.ElementRawDamage.Num(),1)) return false;
        TestEqual(TEXT("Element amount is earned once, including source elemental bonus"),Child.ElementRawDamage[0].RawDamage,Parent.ElementRawDamage[0].RawDamage*.4f,.002f);
    }
    auto Ability=Mixed; UBreakerDamageLibrary::FillSourcePools(Attr,EBreakerDamageDelivery::Ability,Ability);
    TestFalse(TEXT("Ability does not snapshot weapon rewrite"),Ability.bWeaponOverpressure);
    auto Dot=Mixed; Dot.bIsDamageOverTime=true; UBreakerDamageLibrary::FillSourcePools(Attr,EBreakerDamageDelivery::Weapon,Dot);
    TestFalse(TEXT("Periodic source does not snapshot weapon rewrite"),Dot.bWeaponOverpressure);
    // Native rocket blast victims each fund their own splash; children never do.
    A->SetActorLocation(FVector(10000,0,0)); B->SetActorLocation(FVector(10000,150,0));
    C->SetActorLocation(FVector(10000,450,0)); D->SetActorLocation(FVector(10000,900,0));
    Weapon->WeaponDefinition=nullptr; Weapon->SetSlotArchetype(1,EBreakerWeaponArchetype::Rocket); Weapon->ResetAmmunition();
    Fire(); ABreakerRocketProjectile* Rocket=nullptr;
    for(TActorIterator<ABreakerRocketProjectile> It(World);It;++It) if(It->GetOwner()==Player&&!It->HasExploded()) Rocket=*It;
    if(!TestNotNull(TEXT("Actual launcher emitted a rocket"),Rocket)) return false;
    Rocket->SetActorEnableCollision(false);
    if(auto* M=Rocket->FindComponentByClass<UProjectileMovementComponent>()){M->StopMovementImmediately();M->Deactivate();}
    if(!Progression->RespecCore(Reason)) return false;
    Observer->Hits.Reset(); Rocket->Explode(A->GetActorLocation(),A);
    int32 Parents=0,Children=0;
    for(const auto& Hit:Observer->Hits) { if(Hit.bFundedWeaponSplash) ++Children; else ++Parents; }
    TestEqual(TEXT("Emitted rocket retains two ordinary blast victims plus Loud"),Parents,3);
    TestEqual(TEXT("Each weapon blast victim funds its own bounded neighbors"),Children,6);
    // Respec alone cannot reprice funded children; dead source follows the
    // same refusal policy as funded reaction children.
    if(!Progression->PurchaseNode(Tree,Rule->NodeId,Reason)||!Progression->PurchaseNode(Tree,Loud->NodeId,Reason)) return false;
    Mixed.ImpactLocation=A->GetActorLocation();
    UBreakerDamageLibrary::FillSourcePools(Attr,EBreakerDamageDelivery::Weapon,Mixed);
    Observer->Hits.Reset(); Observer->bRevokeOnParent=true;
    A->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Mixed);
    TestTrue(TEXT("Parent callback performs real respec"),Observer->bRespecSucceeded);
    TestFalse(TEXT("Respec callback leaves source alive"),Player->GetCombat()->IsDead());
    if(!TestEqual(TEXT("Prepared ordinary and Loud child survive callback source changes"),Observer->Hits.Num(),3)) return false;
    for(int32 I=1;I<3;++I)
        TestEqual(TEXT("Callback cannot reprice already-earned child"),Observer->Hits[I].Result.RawDamage,Observer->Hits[0].Result.RawDamage*.4f,.002f);
    if(!Progression->PurchaseNode(Tree,Rule->NodeId,Reason)||!Progression->PurchaseNode(Tree,Loud->NodeId,Reason)) return false;
    UBreakerDamageLibrary::FillSourcePools(Attr,EBreakerDamageDelivery::Weapon,Mixed);
    Observer->Hits.Reset(); Observer->bRevokeOnParent=true; Observer->bKillSource=true;
    A->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Mixed);
    TestTrue(TEXT("Death callback also performs real respec"),Observer->bRespecSucceeded);
    TestTrue(TEXT("Parent callback kills actual source"),Player->GetCombat()->IsDead());
    TestEqual(TEXT("Dead source cancels prepared children before dispatch"),Observer->Hits.Num(),1);
    Player->GetCombat()->RestoreVitals();
    if(!Progression->PurchaseNode(Tree,Rule->NodeId,Reason)) return false;
    for(int32 Rank=0;Rank<3;++Rank)
        if(!TestTrue(TEXT("Each authored Concussion rank spends a real earned point"),Progression->PurchaseNode(Tree,Area->NodeId,Reason))) return false;
    const float ExpandedRadius=300.f*FMath::Sqrt(1.18f);
    C->SetActorLocation(A->GetActorLocation()+FVector(0,(300.f+ExpandedRadius)*.5f,0));
    UBreakerDamageLibrary::FillSourcePools(Attr,EBreakerDamageDelivery::Weapon,Mixed);
    TestEqual(TEXT("Three six-percent Concussion ranks snapshot square-root radius"),Mixed.WeaponOverpressureRadius,ExpandedRadius,.002f);
    if(!Progression->RespecCore(Reason)) return false;
    Observer->Hits.Reset(); ACombat->ReceiveDamage(Mixed);
    TestEqual(TEXT("Emitted area snapshot still reaches former outside victim after respec"),Observer->Hits.Num(),3);
    TestFalse(TEXT("New source after respec loses rewrite"),[&](){FBreakerDamageRequest R;UBreakerDamageLibrary::FillSourcePools(Attr,EBreakerDamageDelivery::Weapon,R);return R.bWeaponOverpressure;}());
    return true;
}
#endif
