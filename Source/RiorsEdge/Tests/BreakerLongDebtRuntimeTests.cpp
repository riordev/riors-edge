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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerLongDebtRuntimeTest,"RiorsEdge.Abilities.Caster.LongDebtRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerLongDebtRuntimeTest::RunTest(const FString& Parameters)
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
    FBreakerStatusApplicationSpec Spec;Spec.StatusTag=FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"));
    Spec.Duration=4;Spec.TickInterval=1;Spec.BaseDamagePerTick=1; // O2 PLACEHOLDER, deterministic delivery probe, no player entitlement.
    Status->ApplyStatus(Spec,EBreakerDamageFamily::Physical,Player);
    if(!TestEqual(TEXT("Real status application"),Status->GetActiveStatuses().Num(),1))return false;
    TestEqual(TEXT("Application captures double cadence"),Status->GetActiveStatuses()[0].Spec.TickInterval,.5f,.001f);
    FBreakerStatusApplicationSpec Copied=Status->GetActiveStatuses()[0].Spec;
    if(!TestTrue(TEXT("Real doctrine respec"),Progression->RespecAtForge(Tree->Currency,true,Reason)))return false;
    TestEqual(TEXT("Still negative respec restores existing penalty"),Combat->GetComposedIncomingDamageMultiplier(),1.15f,.001f);
    TestEqual(TEXT("Applied snapshot survives source respec"),Status->GetActiveStatuses()[0].Spec.TickInterval,.5f,.001f);
    const int32 BeforeTicks=Status->GetActiveStatuses()[0].TicksDelivered;
    Status->AdvanceStatuses(1.f);
    TestEqual(TEXT("Two real tick boundaries, not just displayed interval"),Status->GetActiveStatuses()[0].TicksDelivered-BeforeTicks,2);
    Status->ApplyStatus(Copied,EBreakerDamageFamily::Physical,Player);
    TestEqual(TEXT("Copied application never halves twice"),Status->GetActiveStatuses()[0].Spec.TickInterval,.5f,.001f);
    return true;
}
#endif
