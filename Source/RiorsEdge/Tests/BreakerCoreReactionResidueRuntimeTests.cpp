#include "Tests/BreakerReactionRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerElementReactions.h"
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreReactionResidueRuntimeTest,"RiorsEdge.Combat.Elements.CoreReactionResidueRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreReactionResidueRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter=Frame; };
    auto* Player=World->SpawnActor<ABreakerCharacter>(); if (!TestNotNull(TEXT("Player spawned"),Player)) return false; Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* Attr=Player->GetAttributes(); auto* ASC=Player->GetAbilitySystemComponent(); ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attr);
    Player->GetCombat()->BindAttributes(Attr); auto* Progression=Player->GetProgression(); Progression->BindAttributes(Attr);
    auto* Class=NewObject<UBreakerClassDefinition>(); Class->ClassId=EBreakerClassId::Caster;
    auto* Tree=NewObject<UBreakerProgressionTree>(Class); Tree->TreeId=TEXT("Test.Core.ReactionResidue"); Tree->Currency=EBreakerPointCurrency::CorePoints; Class->BranchTrees.Add(Tree);
    auto* Node=NewObject<UBreakerProgressionNode>(Tree); Node->NodeId=TEXT("Test.Core.ReactionResidue.Rule"); Node->Currency=Tree->Currency;
    Tree->Nodes.Add(Node);
    auto* ChainNode=NewObject<UBreakerProgressionNode>(Tree);
    ChainNode->NodeId=TEXT("Test.Core.ReactionResidue.Chain"); ChainNode->Currency=Tree->Currency;
    ChainNode->MaxRank=1; ChainNode->CostPerRank=2;
    ChainNode->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Reaction.Chain")));
    Tree->Nodes.Add(ChainNode);
    Node->MaxRank = 3; Node->CostPerRank = 1;
    FBreakerNodeEffect ResidueEffect; ResidueEffect.StatTarget = EBreakerNodeStatTarget::ReactionResiduePercent;
    ResidueEffect.StatBucket = EBreakerNodeStatBucket::Flat; ResidueEffect.ValuePerRank = 10;
    Node->Effects.Add(ResidueEffect);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5,Progression->ExperienceCurve));
    FText Reason; if (!TestTrue(TEXT("Reaction Chain uses earned Core purchase"),Progression->PurchaseNode(Tree,Node->NodeId,Reason))) return false;
    if (!TestTrue(TEXT("Chain uses separately priced earned notable"),Progression->PurchaseNode(Tree,ChainNode->NodeId,Reason))) return false;
    auto Enemy=[&](float X)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* E=World->SpawnActor<ABreakerEnemy>(FVector(X,0,100),FRotator::ZeroRotator,Spawn);
        if (!E) return static_cast<ABreakerEnemy*>(nullptr);
        E->ConfigureCrowdProbe(); E->DispatchBeginPlay(); E->SetActorTickEnabled(false);
        if (auto* Movement = E->FindComponentByClass<UBreakerEnemyMovementComponent>()) Movement->SetComponentTickEnabled(false);
        auto* Health=FindObject<UBreakerAttributeSet>(E,TEXT("Attributes"));
        E->FindComponentByClass<UBreakerCombatComponent>()->BindAttributes(Health);
        return E;
    };
    auto* Origin=Enemy(1000); auto* Near=Enemy(1200); auto* Far=Enemy(1350);
    if (!TestNotNull(TEXT("Origin spawned"),Origin) || !TestNotNull(TEXT("Near enemy spawned"),Near) || !TestNotNull(TEXT("Far enemy spawned"),Far)) return false;
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
    auto* Foreign=World->SpawnActor<AActor>(); if (!TestNotNull(TEXT("Foreign reactor spawned"),Foreign)) return false;
    FBreakerDamageRequest Trigger; Trigger.BaseDamage=1; Trigger.Element=EBreakerElement::Rift; Trigger.ElementalFraction=1;
    Trigger.DamageFamily=EBreakerDamageFamily::Elemental; Trigger.bCanCritical=false; Trigger.SetInstigator(Foreign);
    const auto Collapse=FGameplayTag::RequestGameplayTag(TEXT("Reaction.Collapse"));
    auto ReactionHits=[&](UBreakerReactionRuntimeObserver* O)
    { auto Hits=O->Hits; Hits.RemoveAll([&](const FBreakerHitContext& H){return H.DamageTypeTag!=Collapse;}); return Hits; };
    if (!TestTrue(TEXT("Ordinary buildup earns source-owned Rot"),Earn())) return false;
    Clock(.23f);
    const auto Before = OriginalStatus->GetActiveStatuses()[0];
    const float N = BreakerElementReactions::RemainingRotBudget(Before);
    const float R = Before.InitialReactionBudget * N / Before.InitialDamageBudget;
    FBreakerDamageResult Accepted; Accepted.RawDamage=1; Accepted.HealthDamage=1;
    const uint64 Token=OriginalStatus->PrepareElementReaction(Trigger,Accepted);
    if (!TestTrue(TEXT("Reaction prepares"),Token!=0) || !TestEqual(TEXT("One finite remainder installed"),OriginalStatus->GetActiveStatuses().Num(),1)) return false;
    const auto Residual = OriginalStatus->GetActiveStatuses()[0];
    TestEqual(TEXT("Ten percent normal budget retained"),Residual.UnpaidDamageBudget,N*.1f,.001f);
    TestEqual(TEXT("Ten percent reaction funding retained"),Residual.InitialReactionBudget,R*.1f,.001f);
    TestEqual(TEXT("Remaining finite duration preserved"),Residual.RemainingDuration,Before.RemainingDuration);
    TestEqual(TEXT("Next tick phase preserved"),Residual.TimeUntilNextTick,Before.TimeUntilNextTick);
    TestEqual(TEXT("Tick intensity scales without new funding"),Residual.Spec.BaseDamagePerTick,Before.Spec.BaseDamagePerTick*.1f,.001f);
    TestTrue(TEXT("Original source attribution retained"),Residual.Instigator==Before.Instigator);
    TestFalse(TEXT("Residual cannot expiry spread"),Residual.bSympathyOnExpiry);
    TestFalse(TEXT("Residual cannot persist"),Residual.bPersistentRot);
    TestEqual(TEXT("Residual cannot fund application procs"),Residual.Spec.ProcCoefficient,0.f);
    if (!Progression->RespecCore(Reason)) return false;
    OriginalStatus->FlushElementReaction(Token); OriginalStatus->FlushElementReaction(Token);
    auto Main=ReactionHits(MainObserver), Child=ReactionHits(ChildObserver);
    if (!TestEqual(TEXT("Exactly one claimed primary"),Main.Num(),1)||!TestEqual(TEXT("Exactly one claimed child"),Child.Num(),1)) return false;
    TestEqual(TEXT("Primary spends only unretained reaction funding"),Main[0].Result.RawDamage,R*.9f,.001f);
    TestEqual(TEXT("Chain half follows reduced primary"),Child[0].Result.RawDamage,R*.45f,.001f);
    MainObserver->Hits.Reset(); ChildObserver->Hits.Reset();
    OriginalCombat->ReceiveDamage(Trigger);
    Main=ReactionHits(MainObserver);
    if (!TestEqual(TEXT("Residual can fund a later reaction"),Main.Num(),1)) return false;
    TestEqual(TEXT("Respec consumes exactly remaining reaction credit"),Main[0].Result.RawDamage,R*.1f,.001f);
    TestFalse(TEXT("No residual after rule removed"),OriginalStatus->HasStatus(Rot));
    TestTrue(TEXT("No fresh Chain after respec"),ChildObserver->Hits.IsEmpty());
    if (!Progression->PurchaseNode(Tree,Node->NodeId,Reason) || !Progression->PurchaseNode(Tree,Node->NodeId,Reason) || !Progression->PurchaseNode(Tree,ChainNode->NodeId,Reason) || !Earn()) return false;
    const float RankedN=OriginalStatus->GetActiveStatuses()[0].UnpaidDamageBudget;
    OriginalCombat->RestoreVitals();
    MainObserver->Combat=OriginalCombat; MainObserver->Status=OriginalStatus; MainObserver->Reactor=Foreign;
    OriginalStatus->OnStatusConsumed.AddDynamic(MainObserver,&UBreakerReactionRuntimeObserver::OnConsumed);
    MainObserver->bReenterOnConsume=true;
    const uint64 RankedToken=OriginalStatus->PrepareElementReaction(Trigger,Accepted);
    TestFalse(TEXT("Real nested consume callback ran"),MainObserver->bReenterOnConsume);
    if (!TestEqual(TEXT("Nested hits cannot create or consume another remainder"),OriginalStatus->GetActiveStatuses().Num(),1)) return false;
    if (!TestEqual(TEXT("Two ranks retain twenty percent"),OriginalStatus->GetActiveStatuses()[0].UnpaidDamageBudget,RankedN*.2f,.001f)) return false;
    OriginalStatus->ConsumeAllStatuses();
    OriginalStatus->FlushElementReaction(RankedToken);
    TestTrue(TEXT("Cleansing prepared remainder cannot be undone by flush"),OriginalStatus->GetActiveStatuses().IsEmpty());
    OriginalCombat->RestoreVitals();
    const auto Erased=FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"));
    FBreakerDamageRequest VoidHit; VoidHit.BaseDamage=OriginalStatus->GetVoidThreshold()/(4*Attr->GetAbilityDamageMultiplier());
    VoidHit.Element=EBreakerElement::Void; VoidHit.ElementalFraction=1; VoidHit.DamageFamily=EBreakerDamageFamily::Elemental;
    VoidHit.bCanCritical=false; VoidHit.SetInstigator(Player); UBreakerDamageLibrary::FillSourcePools(Attr,EBreakerDamageDelivery::Ability,VoidHit);
    for(int32 I=0;I<20&&!OriginalStatus->HasStatus(Erased);++I) OriginalCombat->ReceiveDamage(VoidHit);
    if (!TestTrue(TEXT("Real Void buildup earns Erased"),OriginalStatus->HasStatus(Erased))) return false;
    Clock(.13f);
    const auto VoidBefore=OriginalStatus->GetActiveStatuses()[0];
    const uint64 VoidToken=OriginalStatus->PrepareElementReaction(Trigger,Accepted);
    if (!TestEqual(TEXT("Erased leaves one finite remainder"),OriginalStatus->GetActiveStatuses().Num(),1)) return false;
    const auto VoidAfter=OriginalStatus->GetActiveStatuses()[0];
    TestEqual(TEXT("Erased retains ordinary funding proportionally"),VoidAfter.UnpaidDamageBudget,VoidBefore.UnpaidDamageBudget*.2f,.001f);
    TestEqual(TEXT("Erased delay is not restarted"),VoidAfter.RemainingDuration,VoidBefore.RemainingDuration);
    OriginalStatus->FlushElementReaction(VoidToken);
    MainObserver->Hits.Reset();
    Clock(VoidAfter.RemainingDuration+.03f);
    float VoidPaid=0;
    for (const auto& Hit : MainObserver->Hits) if (Hit.bFromDoT) VoidPaid+=Hit.Result.RawDamage;
    TestEqual(TEXT("Erased remainder expires for retained ordinary damage only"),VoidPaid,VoidAfter.UnpaidDamageBudget,.001f);
    TestFalse(TEXT("Erased remainder ends"),OriginalStatus->HasStatus(Erased));
    return true;
}
#endif