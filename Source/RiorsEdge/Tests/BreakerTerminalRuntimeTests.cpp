#include "Misc/AutomationTest.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerElementReactions.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerTerminalRuntimeTest,"RiorsEdge.Abilities.Caster.TerminalRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerTerminalRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);if(!World)return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GFrameCounter=Frame;};
    auto* Player=World->SpawnActor<ABreakerCharacter>();if(!Player)return false;
    Player->SetActorTickEnabled(false);Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC=Player->GetAbilitySystemComponent();auto* Attributes=Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player,Player);ASC->AddAttributeSetSubobject(Attributes);
    auto* Combat=Player->GetCombat();Combat->BindAttributes(Attributes);
    auto* Progression=Player->GetProgression();Progression->BindAttributes(Attributes);
    if(!Progression->ChoosePermanentClassById(EBreakerClassId::Caster))return false;
    FBreakerQuestFlagSet Flags;for(const auto& Mission:UBreakerMissionLibrary::GetMissions())for(const auto& Beat:Mission.Beats)
        for(const auto& Flag:UBreakerMissionLibrary::BeatCompletionFlags(Beat))Flags.Add(Flag);
    Progression->SettleDoctrineEntitlement(Flags);
    FText Reason;
    const auto* Tree=UBreakerProgressionLibrary::GetCasterVoidWhispererTree();
    for(const TCHAR* Id:{TEXT("Caster.VoidWhisperer.Seep"),TEXT("Caster.VoidWhisperer.StandingWater"),TEXT("Caster.VoidWhisperer.Patience"),TEXT("Caster.VoidWhisperer.Lingering"),TEXT("Caster.VoidWhisperer.Attrition"),TEXT("Caster.VoidWhisperer.Drain")})
        if(!TestTrue(TEXT("Real six-point path"),Progression->PurchaseNode(Tree,Id,Reason)))return false;
    if(!TestTrue(TEXT("Paid Terminal purchase"),Progression->PurchaseNode(Tree,TEXT("Caster.VoidWhisperer.Terminal"),Reason)))return false;
    TestEqual(TEXT("Eight Doctrine points spent"),Progression->GetUnspentPoints(Tree->Currency),0);
    auto MakeTarget=[&]() {
        auto* T=World->SpawnActor<ABreakerCharacter>();
        if(!T)return T;
        T->SetActorTickEnabled(false);T->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* A=T->GetAbilitySystemComponent();A->InitAbilityActorInfo(T,T);A->AddAttributeSetSubobject(T->GetAttributes());
        T->GetCombat()->BindAttributes(T->GetAttributes());
        auto* S=T->FindComponentByClass<UBreakerStatusComponent>();if(!S->HasBegunPlay())S->BeginPlay();S->SetComponentTickEnabled(false);
        FBreakerDamageRequest Lower;Lower.BaseDamage=T->GetAttributes()->GetHealth()-.20f*T->GetCombat()->GetMaxHealth();
        Lower.DamageFamily=EBreakerDamageFamily::TrueDamage;Lower.bCanCritical=false;Lower.bCanBeAvoided=false;Lower.bBypassShield=true;
        T->GetCombat()->ReceiveDamage(Lower);
        return T;
    };
    auto* Target=MakeTarget();if(!Target)return false;
    auto* Status=Target->FindComponentByClass<UBreakerStatusComponent>();
    FBreakerStatusApplicationSpec Spec;Spec.StatusTag=FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"));
    Spec.Duration=4;Spec.TickInterval=1;Spec.BaseDamagePerTick=1; // O2 PLACEHOLDER, deterministic native application witness.
    Spec.Snapshot.SourcePower=1;Spec.Snapshot.CriticalChance=.8f;Spec.Snapshot.CriticalMultiplier=1.5f;
    Spec.Snapshot.bHasCriticalRollSample=true;Spec.Snapshot.CriticalRollSample=.7f;Spec.Snapshot.bRolledCritical=true;
    Status->ApplyStatus(Spec,EBreakerDamageFamily::Physical,Player);
    if(!TestEqual(TEXT("One physical application"),Status->GetActiveStatuses().Num(),1))return false;
    const float InitialPhysical=Status->GetActiveStatuses()[0].TerminalPhysicalBudget;
    TestTrue(TEXT("Below quarter health latches persistence"),Status->GetActiveStatuses()[0].bTerminalPersistent);
    const float Before=Target->GetAttributes()->GetHealth();Status->AdvanceStatuses(8);
    if(!TestEqual(TEXT("Exhausted status remains until cleanse"),Status->GetActiveStatuses().Num(),1))return false;
    TestEqual(TEXT("Persistent physical application pays finite snapshot only"),Before-Target->GetAttributes()->GetHealth(),InitialPhysical*1.5f,.001f);
    TestEqual(TEXT("Original sample is retained"),Status->GetActiveStatuses()[0].Spec.Snapshot.CriticalRollSample,.7f);
    TestEqual(TEXT("Remaining physical funding exhausted"),Status->GetActiveStatuses()[0].TerminalPhysicalBudget,0.f);
    const float Stopped=Target->GetAttributes()->GetHealth();Status->AdvanceStatuses(8);
    TestEqual(TEXT("No damage after exhaustion"),Target->GetAttributes()->GetHealth(),Stopped);
    Target->GetCombat()->ApplyHealingAmount(Target->GetCombat()->GetMaxHealth(), Player, FGameplayTag());
    FBreakerDamageRequest LowerRefreshed;LowerRefreshed.BaseDamage=Target->GetAttributes()->GetHealth()-Target->GetCombat()->GetMaxHealth()*.20f;
    LowerRefreshed.DamageFamily=EBreakerDamageFamily::TrueDamage;LowerRefreshed.bCanCritical=false;
    LowerRefreshed.bCanBeAvoided=false;LowerRefreshed.bBypassShield=true;Target->GetCombat()->ReceiveDamage(LowerRefreshed);
    auto RefreshedSpec=Spec;RefreshedSpec.Snapshot.CriticalRollSample=.99f;RefreshedSpec.Snapshot.bRolledCritical=false;
    Status->ApplyStatus(RefreshedSpec,EBreakerDamageFamily::Physical,Player);
    const float RefundedSchedule=Status->GetActiveStatuses()[0].TerminalPhysicalBudget;
    TestTrue(TEXT("New accepted refresh funds a fresh finite schedule"),RefundedSchedule>0);
    TestEqual(TEXT("Refresh preserves original critical sample"),Status->GetActiveStatuses()[0].Spec.Snapshot.CriticalRollSample,.7f);
    const float RefreshBefore=Target->GetAttributes()->GetHealth();Status->AdvanceStatuses(8);
    TestEqual(TEXT("Refreshed exhausted ailment pays only its newly earned schedule"),RefreshBefore-Target->GetAttributes()->GetHealth(),RefundedSchedule*1.5f,.001f);
    TestEqual(TEXT("Refreshed physical funding exhausts again"),Status->GetActiveStatuses()[0].TerminalPhysicalBudget,0.f);
    Status->ScaleRemainingDurations(0);TestEqual(TEXT("Explicit cleanse removes persistence"),Status->GetActiveStatuses().Num(),0);
    auto* ElementTarget=MakeTarget();if(!ElementTarget)return false;
    auto* ElementStatus=ElementTarget->FindComponentByClass<UBreakerStatusComponent>();
    FBreakerDamageRequest Hit;Hit.BaseDamage=ElementStatus->GetEntropyThreshold()*1.1f;Hit.Element=EBreakerElement::Entropy;
    Hit.ElementalFraction=1;Hit.DamageFamily=EBreakerDamageFamily::Elemental;Hit.bCanCritical=false;Hit.bCanBeAvoided=false;Hit.SetInstigator(Player);
    ElementTarget->GetCombat()->ReceiveDamage(Hit);
    if(!TestEqual(TEXT("Actual accepted hit funds Rot"),ElementStatus->GetActiveStatuses().Num(),1))return false;
    const float Funded=ElementStatus->GetActiveStatuses()[0].InitialDamageBudget;
    // Pure query regression built from the actual funded application: a
    // persistent identity's nominal expiry cannot erase its unpaid credit.
    auto PersistentQuery=ElementStatus->GetActiveStatuses()[0];PersistentQuery.RemainingDuration=0;
    TestEqual(TEXT("Persistent Rot query returns only remaining finite credit"),BreakerElementReactions::RemainingRotBudget(PersistentQuery),PersistentQuery.UnpaidDamageBudget);
    PersistentQuery.bTerminalPersistent=false;PersistentQuery.bPersistentRot=false;
    TestEqual(TEXT("Finite canceled schedule exposes no remaining credit"),BreakerElementReactions::RemainingRotBudget(PersistentQuery),0.f);
    const float ElementBefore=ElementTarget->GetAttributes()->GetHealth();ElementStatus->AdvanceStatuses(8);
    if(!TestEqual(TEXT("Terminal Rot persists"),ElementStatus->GetActiveStatuses().Num(),1))return false;
    TestEqual(TEXT("Persistent Rot pays original finite credit"),ElementBefore-ElementTarget->GetAttributes()->GetHealth(),Funded,.001f);
    TestEqual(TEXT("Persistent Rot cannot replenish reaction credit"),ElementStatus->GetActiveStatuses()[0].UnpaidDamageBudget,0.f);
    const float ElementStopped=ElementTarget->GetAttributes()->GetHealth();ElementStatus->AdvanceStatuses(8);
    TestEqual(TEXT("Exhausted Rot stops paying"),ElementTarget->GetAttributes()->GetHealth(),ElementStopped);
    auto* CrossingTarget=MakeTarget();if(!CrossingTarget)return false;
    CrossingTarget->GetCombat()->RestoreVitals();
    auto* CrossingStatus=CrossingTarget->FindComponentByClass<UBreakerStatusComponent>();
    CrossingTarget->GetCombat()->ReceiveDamage(Hit);
    if(!TestEqual(TEXT("Final-tick witness earns native Rot"),CrossingStatus->GetActiveStatuses().Num(),1))return false;
    const auto CrossingApplication=CrossingStatus->GetActiveStatuses()[0];
    const float Quarter=CrossingTarget->GetCombat()->GetMaxHealth()*.25f;
    const float LastTick=CrossingApplication.Spec.BaseDamagePerTick;
    FBreakerDamageRequest LowerToCrossing;
    LowerToCrossing.BaseDamage=CrossingTarget->GetAttributes()->GetHealth()-(Quarter+CrossingApplication.InitialDamageBudget-LastTick*.5f);
    LowerToCrossing.DamageFamily=EBreakerDamageFamily::TrueDamage;LowerToCrossing.bCanCritical=false;
    LowerToCrossing.bCanBeAvoided=false;LowerToCrossing.bBypassShield=true;
    CrossingTarget->GetCombat()->ReceiveDamage(LowerToCrossing);
    TestTrue(TEXT("Final-tick witness starts above quarter health"),CrossingTarget->GetAttributes()->GetHealth()>Quarter);
    TestFalse(TEXT("Above-quarter Rot has not latched Terminal"),CrossingStatus->GetActiveStatuses()[0].bTerminalPersistent);
    CrossingStatus->AdvanceStatuses(CrossingApplication.RemainingDuration);
    if(!TestEqual(TEXT("Final Rot tick crossing quarter health retains its application"),CrossingStatus->GetActiveStatuses().Num(),1))return false;
    TestTrue(TEXT("Final paid tick crossed below quarter"),CrossingTarget->GetAttributes()->GetHealth()<Quarter);
    TestTrue(TEXT("Final paid tick latches Terminal before expiry removal"),CrossingStatus->GetActiveStatuses()[0].bTerminalPersistent);
    TestEqual(TEXT("Crossing does not mint a further damage budget"),CrossingStatus->GetActiveStatuses()[0].UnpaidDamageBudget,0.f);
    if(!TestTrue(TEXT("Actual doctrine respec"),Progression->RespecAtForge(Tree->Currency,true,Reason)))return false;
    ElementStatus->AdvanceStatuses(.01f);TestEqual(TEXT("Respec cannot maintain old Terminal"),ElementStatus->GetActiveStatuses().Num(),0);
    return true;
}
#endif
