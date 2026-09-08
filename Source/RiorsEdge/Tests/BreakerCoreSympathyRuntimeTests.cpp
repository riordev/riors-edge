#include "Tests/BreakerReactionRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreSympathyRuntimeTest,"RiorsEdge.Combat.Elements.CoreSympathyRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreSympathyRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter=Frame; };
    auto* Player=World->SpawnActor<ABreakerCharacter>(); Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* Attr=Player->GetAttributes(); auto* ASC=Player->GetAbilitySystemComponent(); ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attr);
    Player->GetCombat()->BindAttributes(Attr); auto* Progression=Player->GetProgression(); Progression->BindAttributes(Attr);
    auto* Class=NewObject<UBreakerClassDefinition>(); Class->ClassId=EBreakerClassId::Caster;
    auto* Tree=NewObject<UBreakerProgressionTree>(Class); Tree->TreeId=TEXT("Test.Core.Sympathy"); Tree->Currency=EBreakerPointCurrency::CorePoints; Class->BranchTrees.Add(Tree);
    auto* Node=NewObject<UBreakerProgressionNode>(Tree); Node->NodeId=TEXT("Test.Core.Sympathy.Rule"); Node->Currency=Tree->Currency;
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Sympathy"))); Tree->Nodes.Add(Node);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5,Progression->ExperienceCurve));
    FText Reason; if (!TestTrue(TEXT("Sympathy uses earned Core purchase"),Progression->PurchaseNode(Tree,Node->NodeId,Reason))) return false;
    auto Enemy=[&](float X)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* E=World->SpawnActor<ABreakerEnemy>(FVector(X,0,100),FRotator::ZeroRotator,Spawn);
        E->ConfigureCrowdProbe(); E->DispatchBeginPlay(); E->SetActorTickEnabled(false);
        if (auto* Movement = E->FindComponentByClass<UBreakerEnemyMovementComponent>()) Movement->SetComponentTickEnabled(false);
        auto* Health=FindObject<UBreakerAttributeSet>(E,TEXT("Attributes"));
        E->FindComponentByClass<UBreakerCombatComponent>()->BindAttributes(Health);
        return E;
    };
    auto* Origin=Enemy(1000); auto* Near=Enemy(1200); auto* Far=Enemy(1350);
    auto* OriginalStatus=Origin->FindComponentByClass<UBreakerStatusComponent>();
    auto* NearStatus=Near->FindComponentByClass<UBreakerStatusComponent>(); auto* FarStatus=Far->FindComponentByClass<UBreakerStatusComponent>();
    const auto Rot=FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    auto Clock=[&](float Seconds) { for(int32 I=0;I<FMath::CeilToInt(Seconds*100);++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,.01f); } };
    auto Apply=[&]()
    {
        Origin->FindComponentByClass<UBreakerCombatComponent>()->RestoreVitals();
        FBreakerDamageRequest Hit;
        Hit.BaseDamage=OriginalStatus->GetEntropyThreshold() / (4.f * Attr->GetAbilityDamageMultiplier());
        Hit.Element=EBreakerElement::Entropy; Hit.ElementalFraction=1;
        Hit.DamageFamily=EBreakerDamageFamily::Elemental; Hit.bCanCritical=false; Hit.SetInstigator(Player);
        UBreakerDamageLibrary::FillSourcePools(Attr,EBreakerDamageDelivery::Ability,Hit);
        for(int32 I=0;I<20 && !OriginalStatus->HasStatus(Rot);++I) Origin->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Hit);
        return OriginalStatus->HasStatus(Rot);
    };
    if (!TestTrue(TEXT("Real hit buildup activates Rot"),Apply())) return false;
    const auto Original=OriginalStatus->GetActiveStatuses()[0];
    auto* Observer=NewObject<UBreakerReactionRuntimeObserver>(Origin); Observer->Status=OriginalStatus; Observer->bAdvanceOnRotTick=true;
    Origin->FindComponentByClass<UBreakerCombatComponent>()->OnDamageTaken.AddDynamic(Observer,&UBreakerReactionRuntimeObserver::OnHit);
    if (!Progression->RespecCore(Reason)) return false;
    Clock(Original.RemainingDuration+.03f);
    TestFalse(TEXT("Expired original is removed before spreading"),OriginalStatus->HasStatus(Rot));
    if (!TestTrue(TEXT("Application snapshot spreads after source respec"),NearStatus->HasStatus(Rot))) return false;
    TestFalse(TEXT("Only nearest target receives copy"),FarStatus->HasStatus(Rot));
    const auto Copy=NearStatus->GetActiveStatuses()[0];
    TestEqual(TEXT("Copy retains original finite damage budget"),Copy.InitialDamageBudget,Original.InitialDamageBudget,.001f);
    TestEqual(TEXT("Copy retains funded reaction credit"),Copy.InitialReactionBudget,Original.InitialReactionBudget,.001f);
    TestTrue(TEXT("Copy preserves reward owner"),Copy.Instigator.Get()==Player);
    TestFalse(TEXT("Copy cannot recursively expiry-spread"),Copy.bSympathyOnExpiry);
    TestEqual(TEXT("Copied status has no recursive procs"),Copy.Spec.ProcCoefficient,0.f);
    Clock(Copy.RemainingDuration+.03f);
    TestFalse(TEXT("Copy expires finitely"),NearStatus->HasStatus(Rot));
    TestFalse(TEXT("No second generation appears"),FarStatus->HasStatus(Rot));
    if (!Progression->PurchaseNode(Tree,Node->NodeId,Reason)||!Apply()) return false;
    OriginalStatus->ConsumeAllStatuses(); Clock(Original.RemainingDuration+.03f);
    TestFalse(TEXT("Consumed Rot does not expiry-spread"),NearStatus->HasStatus(Rot));
    if (!Apply()) return false;
    OriginalStatus->ScaleRemainingDurations(0); Clock(Original.RemainingDuration+.03f);
    TestFalse(TEXT("Cleansed Rot does not expiry-spread"),NearStatus->HasStatus(Rot));
    return true;
}
#endif
