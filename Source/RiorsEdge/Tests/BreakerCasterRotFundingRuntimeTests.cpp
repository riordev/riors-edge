#include "Combat/BreakerZoneActor.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerElementSourceMath.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "Classes/BreakerCasterStatusRules.h"
#include "Misc/AutomationTest.h"
#include "Combat/BreakerStatusComponent.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Abilities/BreakerAbility_Unmake.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerExperience.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCasterRotFundingRuntimeTest,"RiorsEdge.Abilities.Caster.RotFundingRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCasterRotFundingRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);if(!World)return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GFrameCounter=Frame;};
    auto Clock=[&](float Seconds){for(float T=0;T<Seconds;T+=.01f){++GFrameCounter;World->Tick(LEVELTICK_All,.01f);}};
    auto* Player=World->SpawnActor<ABreakerCharacter>();if(!Player)return false;
    Player->SetActorTickEnabled(false);Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC=Player->GetAbilitySystemComponent();auto* Attributes=Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player,Player);ASC->AddAttributeSetSubobject(Attributes);
    auto* Combat=Player->GetCombat();Combat->BindAttributes(Attributes);
    auto* Progression=Player->GetProgression();Progression->BindAttributes(Attributes);
    if(!Progression->ChoosePermanentClassById(EBreakerClassId::Caster))return false;
    auto* Mana=Player->GetMana();Mana->BindAttributes(Attributes);
    auto* State=UBreakerAbilityStateComponent::FindOrAdd(Player);
    State->RegisterAllComponentTickFunctions(true);State->SetComponentTickEnabled(true);if(!State->HasBegunPlay())State->BeginPlay();
    auto* Abilities=Player->GetAbilities();FText Reason;
    if(!Abilities->TryEquipAbility(EBreakerAbilitySlot::ClassAbilityOne,TEXT("Caster.Cleave"),Reason))return false;
    Abilities->RefreshGrants();
    // Native paid starter casts reach debt; no attribute write or extra resource grant.
    for(int32 Cast=0;Cast<20 && !Mana->IsOvercast();++Cast)
    {
        if(!TestTrue(TEXT("Paid Cleave casts"),Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityOne)))return false;
        Clock(.55f);
    }
    if(!TestTrue(TEXT("Paid casting reaches negative Mana"),Mana->IsOvercast()))return false;
    FBreakerQuestFlagSet Flags;for(const auto& Mission:UBreakerMissionLibrary::GetMissions())for(const auto& Beat:Mission.Beats)
        for(const auto& Flag:UBreakerMissionLibrary::BeatCompletionFlags(Beat))Flags.Add(Flag);
    Progression->SettleDoctrineEntitlement(Flags);
    const auto* Tree=UBreakerProgressionLibrary::GetCasterVoidWhispererTree();
    for(const TCHAR* Id:{TEXT("Caster.VoidWhisperer.Seep"),TEXT("Caster.VoidWhisperer.StandingWater"),TEXT("Caster.VoidWhisperer.Patience"),TEXT("Caster.VoidWhisperer.Lingering"),TEXT("Caster.VoidWhisperer.Attrition"),TEXT("Caster.VoidWhisperer.Drain")})
        if(!TestTrue(TEXT("Real six-point path"),Progression->PurchaseNode(Tree,Id,Reason)))return false;
    TestEqual(TEXT("Before purchase existing penalty"),Combat->GetComposedIncomingDamageMultiplier(),1.15f,.001f);
    if(!Progression->PurchaseNode(Tree,TEXT("Caster.VoidWhisperer.LongDebt"),Reason))return false;
    TestEqual(TEXT("Real eight-point route spends wallet"),Progression->GetUnspentPoints(Tree->Currency),0);
    TestEqual(TEXT("Live negative acquisition replaces penalty"),Combat->GetComposedIncomingDamageMultiplier(),1.25f,.001f);
    FBreakerDamageRequest Incoming;Incoming.BaseDamage=1;Incoming.bCanCritical=false;Incoming.bCanBeAvoided=false;Incoming.DamageFamily=EBreakerDamageFamily::TrueDamage;
    TestEqual(TEXT("Actual native incoming hit"),Combat->ReceiveDamage(Incoming).HealthDamage,1.25f,.001f);
    auto* Target=World->SpawnActor<ABreakerCharacter>();if(!Target)return false;
    Target->SetActorTickEnabled(false);Target->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* TargetASC=Target->GetAbilitySystemComponent();TargetASC->InitAbilityActorInfo(Target,Target);TargetASC->AddAttributeSetSubobject(Target->GetAttributes());
    Target->GetCombat()->BindAttributes(Target->GetAttributes());
    auto* Status=Target->FindComponentByClass<UBreakerStatusComponent>();if(!Status)return false;
    if(!Status->HasBegunPlay())Status->BeginPlay();Status->SetComponentTickEnabled(false);
    // Native hit kernel and accepted Entropy application; paid Cleave above supplies actual debt.
    FBreakerDamageRequest Hit;Hit.BaseDamage=Status->GetEntropyThreshold();Hit.Element=EBreakerElement::Entropy;
    Hit.ElementalFraction=1;Hit.DamageFamily=EBreakerDamageFamily::Elemental;Hit.SetInstigator(Player);Hit.CriticalChance=1;
    Hit.bCanBeAvoided=false;
    const auto Result=Target->GetCombat()->ReceiveDamage(Hit);
    if(!TestTrue(TEXT("Kernel records original applying sample"),Result.bHasCriticalRollSample))return false;
    const auto Rot=FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    const auto* Active=Status->GetActiveStatuses().FindByPredicate([&](const auto& A){return A.Spec.StatusTag==Rot;});
    if(!TestNotNull(TEXT("Real accepted hit earns finite Rot"),Active))return false;
    const float Duration=Active->Spec.Duration;const float Budget=Active->InitialDamageBudget;
    TestEqual(TEXT("Rot carries original kernel sample"),Active->Spec.Snapshot.CriticalRollSample,Result.CriticalRollSample);
    TestEqual(TEXT("Long Debt lifetime rule captured while actually negative"),BreakerCasterStatusRules::RotLifetimeMultiplier(Player),2.f);
    TestEqual(TEXT("Longer Rot redistributes finite budget"),Active->Spec.BaseDamagePerTick*FMath::FloorToInt(Active->Spec.Duration/Active->Spec.TickInterval),Budget,.001f);
    TestEqual(TEXT("Already-critical application never pays another critical multiplier"),BreakerCasterStatusRules::RotCriticalBudgetMultiplier(Hit,Result),1.f);
    if(!Progression->RespecAtForge(Tree->Currency,true,Reason))return false;
    TestEqual(TEXT("Respec removes lifetime rule for future hits"),BreakerCasterStatusRules::RotLifetimeMultiplier(Player),1.f);
    TestEqual(TEXT("Existing duration snapshot survives respec"),Status->GetActiveStatuses()[0].Spec.Duration,Duration);
    TestEqual(TEXT("Existing funding survives source changes"),Status->GetActiveStatuses()[0].InitialDamageBudget,Budget);
    for(const TCHAR* Id:{TEXT("Caster.VoidWhisperer.Seep"),TEXT("Caster.VoidWhisperer.StandingWater"),TEXT("Caster.VoidWhisperer.Patience"),TEXT("Caster.VoidWhisperer.Attrition"),TEXT("Caster.VoidWhisperer.Zonework"),TEXT("Caster.VoidWhisperer.SnapshotDiscipline")})
        if(!Progression->PurchaseNode(Tree,Id,Reason))return false;
    bool Found=false;Status->ConsumeStatus(Rot,Found);Target->GetCombat()->RestoreVitals();
    Hit.CriticalChance=.1f; // O2 PLACEHOLDER, deterministic kernel critical boundary.
    for(int32 Seed=0;Seed<10000;++Seed)
    {FRandomStream Probe(Seed);const float Sample=Probe.FRand();if(Sample>=.1f&&Sample<.35f){Hit.RandomSeed=Seed;break;}}
    const auto OutsideResult=Target->GetCombat()->ReceiveDamage(Hit);
    const auto* OutsideRot=Status->GetActiveStatuses().FindByPredicate([&](const auto& A){return A.Spec.StatusTag==Rot;});
    if(!TestNotNull(TEXT("Outside zone actual Rot baseline"),OutsideRot))return false;
    const float OutsideBudget=OutsideRot->InitialDamageBudget;
    Status->ConsumeStatus(Rot,Found);Target->GetCombat()->RestoreVitals();
    auto* Zone=World->SpawnActor<ABreakerZoneActor>();if(!Zone)return false;
    FBreakerZoneSpec ZoneSpec;ZoneSpec.RadiusCm=200;ZoneSpec.Duration=10; // O2 PLACEHOLDER, owned-zone geometry witness.
    Zone->SetActorLocation(Player->GetActorLocation());Zone->ConfigureZone(ZoneSpec,Player);
    const auto InsideResult=Target->GetCombat()->ReceiveDamage(Hit);
    const auto* InsideRot=Status->GetActiveStatuses().FindByPredicate([&](const auto& A){return A.Spec.StatusTag==Rot;});
    if(!TestNotNull(TEXT("Inside zone actual Rot"),InsideRot))return false;
    TestFalse(TEXT("Original direct hit remains noncritical"),InsideResult.bCritical);
    TestEqual(TEXT("Direct hit damage unchanged by status-only crit"),InsideResult.RawDamage,OutsideResult.RawDamage,.001f);
    TestEqual(TEXT("Kernel preserves original sample across equal seeds"),InsideResult.CriticalRollSample,OutsideResult.CriticalRollSample);
    TestEqual(TEXT("Status replaces one critical factor using same sample"),InsideRot->InitialDamageBudget,OutsideBudget*Hit.CriticalMultiplier,.001f);
    // Native O231 target: only enemy components carry the per-source lockout.
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(30,Progression->ExperienceCurve));
    const auto* Core=UBreakerProgressionLibrary::GetCoreSliceTree();
    for(const UBreakerProgressionNode* Node:Core->Nodes)
        if(Node->Constellation==FName(TEXT("Reaction")))
            for(int32 Rank=0;Rank<Node->MaxRank;++Rank)
                if(!TestTrue(TEXT("XP-funded native Reaction purchase"),Progression->PurchaseNode(Core,Node->NodeId,Reason)))return false;
    TestEqual(TEXT("Actual full Reaction major costs26"),Progression->GetConstellationInvestment(Core,TEXT("Reaction")),26);
    FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Enemy=World->SpawnActor<ABreakerEnemy>(FVector(6000,0,100),FRotator::ZeroRotator,Spawn);if(!Enemy)return false;
    Enemy->ConfigureCrowdProbe();Enemy->SetAreaLevel(40);Enemy->DispatchBeginPlay();Enemy->SetActorTickEnabled(false);
    if(auto* Move=Enemy->FindComponentByClass<UBreakerEnemyMovementComponent>())Move->SetComponentTickEnabled(false);
    auto* EnemyCombat=Enemy->FindComponentByClass<UBreakerCombatComponent>();
    auto* EnemyStatus=Enemy->FindComponentByClass<UBreakerStatusComponent>();
    if(!EnemyCombat||!EnemyStatus)return false;
    EnemyCombat->BindAttributes(FindObject<UBreakerAttributeSet>(Enemy,TEXT("Attributes")));
    auto DeferredHit=Hit;
    const float Acceptance=BreakerElementSource::BuildupMultiplier(DeferredHit)*BreakerElementSource::ResistanceFactor(DeferredHit,EnemyStatus->GetEntropyResistancePercent());
    if(!TestTrue(TEXT("Native enemy accepts Entropy buildup"),Acceptance>0))return false;
    DeferredHit.BaseDamage=1.1f*BreakerElementSource::Threshold(DeferredHit,EnemyStatus->GetEntropyThreshold())/Acceptance;
    const auto InitialResult=EnemyCombat->ReceiveDamage(DeferredHit);
    const auto* InitialRot=EnemyStatus->GetActiveStatuses().FindByPredicate([&](const auto& A){return A.Spec.StatusTag==Rot;});
    if(!TestNotNull(TEXT("Owned-zone hit earns reaction fuel on native enemy"),InitialRot))return false;
    const float ExpectedDeferredBudget=InitialRot->InitialDamageBudget;
    const float InitialCriticalFactor=BreakerCasterStatusRules::RotCriticalBudgetMultiplier(DeferredHit,InitialResult);
    TestEqual(TEXT("Original owned-zone status snapshot promotes the recorded sample"),InitialCriticalFactor,DeferredHit.CriticalMultiplier,.001f);
    auto Trigger=DeferredHit;Trigger.Element=EBreakerElement::Rift;Trigger.BaseDamage=1;Trigger.bCanCritical=false;
    EnemyCombat->ReceiveDamage(Trigger);
    EnemyStatus->ConsumeStatus(Rot,Found); // Remove only finite Residue; preserve O231 lockout.
    EnemyCombat->RestoreVitals();
    const auto DeferredResult=EnemyCombat->ReceiveDamage(DeferredHit);
    TestFalse(TEXT("Owned-zone Rot is deferred during Sympathetic lockout"),EnemyStatus->HasStatus(Rot));
    TestTrue(TEXT("Native deferred buildup crosses its unchanged threshold"),EnemyStatus->GetEntropyBuildup()>=EnemyStatus->GetEntropyThreshold());
    TestEqual(TEXT("Deferred hit retains original noncritical damage"),DeferredResult.RawDamage,InitialResult.RawDamage,.001f);
    TestEqual(TEXT("Deferred hit's recorded sample is unchanged"),DeferredResult.CriticalRollSample,InitialResult.CriticalRollSample);
    if(!TestTrue(TEXT("Actual Doctrine respec before lockout ends"),Progression->RespecAtForge(Tree->Currency,true,Reason)))return false;
    Zone->Destroy();
    TestEqual(TEXT("Future hits no longer receive owned-zone promotion"),BreakerCasterStatusRules::RotCriticalBudgetMultiplier(DeferredHit,DeferredResult),1.f);
    EnemyStatus->AdvanceStatuses(3.f);
    const auto* Resumed=EnemyStatus->GetActiveStatuses().FindByPredicate([&](const auto& A){return A.Spec.StatusTag==Rot;});
    if(!TestNotNull(TEXT("Funded Rot resumes after source leaves its zone and respecs"),Resumed))return false;
    TestEqual(TEXT("Resumed Rot retains exactly the originally promoted budget"),Resumed->InitialDamageBudget,ExpectedDeferredBudget,.001f);
    TestEqual(TEXT("Deferred Rot preserves original applying-hit sample"),Resumed->Spec.Snapshot.CriticalRollSample,DeferredResult.CriticalRollSample);
    TestTrue(TEXT("Deferred Rot retains original applying source"),Resumed->Instigator.Get()==Player);
    return true;
}
#endif
