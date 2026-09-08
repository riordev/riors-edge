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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreReactionChainRuntimeTest,"RiorsEdge.Combat.Elements.CoreReactionChainRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreReactionChainRuntimeTest::RunTest(const FString& Parameters)
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
    auto* Tree=NewObject<UBreakerProgressionTree>(Class); Tree->TreeId=TEXT("Test.Core.ReactionChain"); Tree->Currency=EBreakerPointCurrency::CorePoints; Class->BranchTrees.Add(Tree);
    auto* Node=NewObject<UBreakerProgressionNode>(Tree); Node->NodeId=TEXT("Test.Core.ReactionChain.Rule"); Node->Currency=Tree->Currency;
    Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Reaction.Chain"))); Tree->Nodes.Add(Node);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5,Progression->ExperienceCurve));
    FText Reason; if (!TestTrue(TEXT("Reaction Chain uses earned Core purchase"),Progression->PurchaseNode(Tree,Node->NodeId,Reason))) return false;
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
    auto* OriginalCombat=Origin->FindComponentByClass<UBreakerCombatComponent>();
    auto* NearCombat=Near->FindComponentByClass<UBreakerCombatComponent>();
    auto* FarCombat=Far->FindComponentByClass<UBreakerCombatComponent>();
    auto Observe=[&](AActor* Actor,UBreakerCombatComponent* Combat)
    { auto* O=NewObject<UBreakerReactionRuntimeObserver>(Actor); Combat->OnDamageTaken.AddDynamic(O,&UBreakerReactionRuntimeObserver::OnHit); return O; };
    auto* MainObserver=Observe(Origin,OriginalCombat); auto* ChildObserver=Observe(Near,NearCombat); auto* FarObserver=Observe(Far,FarCombat);
    auto Earn=[&]()
    {
        OriginalStatus->ConsumeAllStatuses(); OriginalCombat->RestoreVitals(); NearCombat->RestoreVitals(); FarCombat->RestoreVitals();
        FBreakerDamageRequest Hit; Hit.BaseDamage=OriginalStatus->GetEntropyThreshold()/(4*Attr->GetAbilityDamageMultiplier());
        Hit.Element=EBreakerElement::Entropy; Hit.ElementalFraction=1; Hit.DamageFamily=EBreakerDamageFamily::Elemental;
        Hit.bCanCritical=false; Hit.SetInstigator(Player); UBreakerDamageLibrary::FillSourcePools(Attr,EBreakerDamageDelivery::Ability,Hit);
        for(int32 I=0;I<20&&!OriginalStatus->HasStatus(Rot);++I) OriginalCombat->ReceiveDamage(Hit);
        MainObserver->Hits.Reset(); ChildObserver->Hits.Reset(); FarObserver->Hits.Reset();
        return OriginalStatus->HasStatus(Rot);
    };
    auto* Foreign=World->SpawnActor<AActor>();
    FBreakerDamageRequest Trigger; Trigger.BaseDamage=1; Trigger.Element=EBreakerElement::Rift; Trigger.ElementalFraction=1;
    Trigger.DamageFamily=EBreakerDamageFamily::Elemental; Trigger.bCanCritical=false; Trigger.SetInstigator(Foreign);
    const auto Collapse=FGameplayTag::RequestGameplayTag(TEXT("Reaction.Collapse"));
    auto ReactionHits=[&](UBreakerReactionRuntimeObserver* O)
    { auto Hits=O->Hits; Hits.RemoveAll([&](const FBreakerHitContext& H){return H.DamageTypeTag!=Collapse;}); return Hits; };
    if (!TestTrue(TEXT("Ordinary buildup earns source-owned Rot"),Earn())) return false;
    const float Budget=OriginalStatus->GetActiveStatuses()[0].InitialReactionBudget;
    OriginalCombat->ReceiveDamage(Trigger);
    const auto Main=ReactionHits(MainObserver), Child=ReactionHits(ChildObserver);
    if (!TestEqual(TEXT("One primary reaction"),Main.Num(),1)||!TestEqual(TEXT("One nearest child reaction"),Child.Num(),1)) return false;
    TestEqual(TEXT("Primary keeps full earned budget"),Main[0].Result.RawDamage,Budget,.001f);
    TestEqual(TEXT("Child pays exactly half with no source recomposition"),Child[0].Result.RawDamage,Budget*.5f,.001f);
    TestTrue(TEXT("Foreign trigger does not steal original credit"),Child[0].Instigator.Get()==Player);
    TestTrue(TEXT("Child uses derived payout route"),Child[0].bFromDoT);
    TestFalse(TEXT("Child rolls no new critical"),Child[0].Result.bCritical);
    TestTrue(TEXT("Child cannot recurse to another enemy"),FarObserver->Hits.IsEmpty());
    TestEqual(TEXT("Child earns no Rift buildup"),NearStatus->GetRiftBuildup(),0.f);
    if (!Earn()) return false;
    // Exercise the real prepare/flush transaction seam: snapshot the rule
    // before consume callbacks, then remove it before deferred delivery.
    FBreakerDamageResult Accepted; Accepted.RawDamage=1; Accepted.HealthDamage=1;
    const uint64 Token=OriginalStatus->PrepareElementReaction(Trigger,Accepted);
    if (!TestTrue(TEXT("Actual status prepares reaction"),Token!=0)) return false;
    if (!Progression->RespecCore(Reason)) return false;
    OriginalStatus->FlushElementReaction(Token); OriginalStatus->FlushElementReaction(Token);
    TestEqual(TEXT("Prepared child survives respec but duplicate flush cannot repay"),ReactionHits(ChildObserver).Num(),1);
    if (!Earn()) return false;
    OriginalCombat->ReceiveDamage(Trigger);
    TestTrue(TEXT("New reaction after respec has no child"),ChildObserver->Hits.IsEmpty());
    if (!Progression->PurchaseNode(Tree,Node->NodeId,Reason)||!Earn()) return false;
    const uint64 CancelToken=OriginalStatus->PrepareElementReaction(Trigger,Accepted);
    Near->Destroy(); OriginalStatus->FlushElementReaction(CancelToken);
    TestTrue(TEXT("Destroyed reserved child is not replaced or paid"),ChildObserver->Hits.IsEmpty()&&FarObserver->Hits.IsEmpty());
    return true;
}
#endif
