#include "Tests/BreakerCoreLoudRuntimeObserver.h"
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

void UBreakerCoreLoudRuntimeObserver::OnHit(const FBreakerHitContext& Hit)
{
    if (!Hit.bFromDoT) Hits.Add(Hit);
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreLoudRuntimeTest,"RiorsEdge.Weapons.CoreLoudRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreLoudRuntimeTest::RunTest(const FString& Parameters)
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
    auto* Tree=NewObject<UBreakerProgressionTree>(Class); Tree->TreeId=TEXT("Test.Core.Loud"); Tree->Currency=EBreakerPointCurrency::CorePoints;
    Class->BranchTrees.Add(Tree);
    auto* Node=NewObject<UBreakerProgressionNode>(Tree); Node->NodeId=TEXT("Test.Core.Loud.Rule"); Node->Currency=Tree->Currency;
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Ballistics.Loud"))); Tree->Nodes.Add(Node);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(2,Progression->ExperienceCurve));
    FText Reason;
    if (!TestTrue(TEXT("Level-earned point purchases Loud"),Progression->PurchaseNode(Tree,Node->NodeId,Reason))) return false;
    FBreakerDamageRequest Snapshot; Snapshot.SetInstigator(Player);
    UBreakerDamageLibrary::FillSourcePools(Attr,EBreakerDamageDelivery::Weapon,Snapshot);
    TestTrue(TEXT("Owned direct weapon source captures Loud"),Snapshot.bWeaponSplashAdditionalTarget);
    auto Ability=Snapshot; UBreakerDamageLibrary::FillSourcePools(Attr,EBreakerDamageDelivery::Ability,Ability);
    TestFalse(TEXT("Ability source clears copied Loud"),Ability.bWeaponSplashAdditionalTarget);
    auto Periodic=Snapshot; Periodic.bIsDamageOverTime=true;
    UBreakerDamageLibrary::FillSourcePools(Attr,EBreakerDamageDelivery::Weapon,Periodic);
    TestFalse(TEXT("Derived periodic source cannot capture Loud"),Periodic.bWeaponSplashAdditionalTarget);
    auto* Weapon=Player->GetWeapon(); Weapon->WeaponDefinition=nullptr;
    Weapon->SetSlotArchetype(1,EBreakerWeaponArchetype::Rocket); Weapon->ResetAmmunition();
    const float Radius=Weapon->GetActiveDefinition()->ExplosionRadius;
    const FVector Impact(10000,0,0);
    auto* Observer=NewObject<UBreakerCoreLoudRuntimeObserver>();
    auto SpawnEnemy=[&](const FVector& Position)
    {
        auto* E=World->SpawnActor<ABreakerEnemy>(Position,FRotator::ZeroRotator); if (!E) return E;
        E->SetAreaLevel(100); E->ConfigureCrowdProbe(); E->DispatchBeginPlay(); E->SetActorTickEnabled(false);
        if (auto* M=E->FindComponentByClass<UBreakerEnemyMovementComponent>()) M->SetComponentTickEnabled(false);
        if (auto* S=E->FindComponentByClass<UBreakerStatusComponent>()) S->SetComponentTickEnabled(false);
        auto* C=E->FindComponentByClass<UBreakerCombatComponent>(); C->SetComponentTickEnabled(false);
        C->OnDamageTaken.AddDynamic(Observer,&UBreakerCoreLoudRuntimeObserver::OnHit);
        return E;
    };
    auto* InsideA=SpawnEnemy(Impact);
    auto* InsideB=SpawnEnemy(Impact+FVector(0,Radius*.5f,0));
    auto* OutsideA=SpawnEnemy(Impact+FVector(Radius*1.25f,0,0));
    auto* OutsideB=SpawnEnemy(Impact+FVector(-Radius*1.75f,0,0));
    auto* Beyond=SpawnEnemy(Impact+FVector(0,-Radius*2.25f,0));
    if (!InsideA||!InsideB||!OutsideA||!OutsideB||!Beyond) return false;
    auto Clock=[&](float Seconds) { for (int32 I=0;I<FMath::CeilToInt(Seconds*100);++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,.01f); } };
    TSet<ABreakerRocketProjectile*> Seen;
    auto Launch=[&]()
    {
        if (Weapon->GetMagazineAmmo()==0) { Weapon->StartReload(); Clock(5); }
        Clock(FMath::Max(1.f,60.f/Weapon->GetEffectiveRoundsPerMinute(Weapon->GetActiveDefinition())+.05f));
        const int32 Ammo=Weapon->GetMagazineAmmo(); Weapon->StartFire(); Weapon->StopFire();
        TestEqual(TEXT("Native rocket pull spends actual ammunition"),Weapon->GetMagazineAmmo(),Ammo-1);
        TArray<ABreakerRocketProjectile*> NewRockets;
        for (TActorIterator<ABreakerRocketProjectile> It(World);It;++It)
            if (It->GetOwner()==Player&&!Seen.Contains(*It))
            {
                Seen.Add(*It); NewRockets.Add(*It); It->SetActorEnableCollision(false);
                if (auto* M=It->FindComponentByClass<UProjectileMovementComponent>()) { M->StopMovementImmediately(); M->Deactivate(); }
            }
        TestEqual(TEXT("Unmodified default launcher emits one rocket"),NewRockets.Num(),1);
        return NewRockets.Num()==1?NewRockets[0]:nullptr;
    };
    auto Check=[&](AActor* Expected,float Factor,float Base,float Crit)
    {
        int32 Count=0;
        for (const auto& Hit:Observer->Hits) if (Hit.Target==Expected)
        {
            ++Count;
            TestEqual(TEXT("Actual blast uses normal or clamped edge falloff"),Hit.Result.RawDamage,
                Base*Factor*(Hit.Result.bCritical?Crit:1.f),.02f);
        }
        TestEqual(TEXT("Expected target receives exactly one blast hit"),Count,1);
    };
    const float Base=Weapon->GetScaledBaseDamage()*Attr->GetDamageMultiplier(), Crit=Attr->GetCriticalMultiplier();
    auto* First=Launch(); if (!First) return false;
    First->Explode(Impact,InsideA);
    TestEqual(TEXT("Two ordinary victims plus exactly one nearby additional victim"),Observer->Hits.Num(),3);
    Check(InsideA,1,Base,Crit); Check(InsideB,FMath::Lerp(1.f,First->EdgeDamageFraction,.5f),Base,Crit);
    Check(OutsideA,First->EdgeDamageFraction,Base,Crit);
    TestFalse(TEXT("Farther outside target and beyond-two-radius target are excluded"),
        Observer->Hits.ContainsByPredicate([&](const FBreakerHitContext& H) { return H.Target==OutsideB||H.Target==Beyond; }));
    // Equal distances choose stable actor identity, regardless direct-hit sorting.
    OutsideA->SetActorLocation(Impact+FVector(Radius*1.5f,0,0));
    OutsideB->SetActorLocation(Impact+FVector(-Radius*1.5f,0,0));
    AActor* TieWinner=OutsideA->GetUniqueID()<OutsideB->GetUniqueID()?OutsideA:OutsideB;
    auto* Second=Launch(); if (!Second) return false;
    if (!TestTrue(TEXT("Real respec happens after emission"),Progression->RespecCore(Reason))) return false;
    auto NewSource=Snapshot; UBreakerDamageLibrary::FillSourcePools(Attr,EBreakerDamageDelivery::Weapon,NewSource);
    TestFalse(TEXT("Post-respec source no longer captures Loud"),NewSource.bWeaponSplashAdditionalTarget);
    Observer->Hits.Reset(); Second->Explode(Impact,InsideA);
    TestEqual(TEXT("Emitted Loud remains live after source respec"),Observer->Hits.Num(),3);
    Check(InsideA,1,Base,Crit); Check(InsideB,FMath::Lerp(1.f,Second->EdgeDamageFraction,.5f),Base,Crit);
    Check(TieWinner,Second->EdgeDamageFraction,Base,Crit);
    const float NewBase=Weapon->GetScaledBaseDamage()*Attr->GetDamageMultiplier(), NewCrit=Attr->GetCriticalMultiplier();
    auto* Third=Launch(); if (!Third) return false;
    Observer->Hits.Reset(); Third->Explode(Impact,InsideA);
    TestEqual(TEXT("New unowned rocket retains both ordinary victims but no extra"),Observer->Hits.Num(),2);
    Check(InsideA,1,NewBase,NewCrit); Check(InsideB,FMath::Lerp(1.f,Third->EdgeDamageFraction,.5f),NewBase,NewCrit);
    return true;
}
#endif
