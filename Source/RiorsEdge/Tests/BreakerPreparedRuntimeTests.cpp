#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerPreparedRuntimeTest,"RiorsEdge.Abilities.Caster.PreparedRuntime",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBreakerPreparedRuntimeTest::RunTest(const FString& Parameters)
{
    for(bool bBuyPrepared:{false,true})
    {
        UWorld::InitializationValues Init;Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);if(!World)return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
        ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
        auto* Player=World->SpawnActor<ABreakerCharacter>();if(!Player)return false;
        Player->SetActorTickEnabled(false);Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* ASC=Player->GetAbilitySystemComponent();auto* Attr=Player->GetAttributes();ASC->InitAbilityActorInfo(Player,Player);ASC->AddAttributeSetSubobject(Attr);
        Player->GetCombat()->BindAttributes(Attr);auto* Progression=Player->GetProgression();Progression->BindAttributes(Attr);
        if(!Progression->ChoosePermanentClassById(EBreakerClassId::Caster))return false;
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(6,Progression->ExperienceCurve));
        auto* Mana=Player->GetMana();Mana->BindAttributes(Attr);Mana->SetComponentTickEnabled(false);
        const auto* Tree=UBreakerProgressionLibrary::GetCasterMultispellTree();const FName Prepared(TEXT("Caster.Multispell.Prepared"));
        const auto* Node=Tree->FindNode(Prepared);if(!TestNotNull(TEXT("Authored Prepared exists"),Node))return false;
        TestEqual(TEXT("Tier four"),Node->Tier,4);TestEqual(TEXT("One rank"),Node->MaxRank,1);TestEqual(TEXT("Two points"),Node->CostPerRank,2);TestEqual(TEXT("Six invested"),Node->RequiredTreeInvestment,6);
        if(!TestEqual(TEXT("One historical prerequisite"),Node->Prerequisites.Num(),1))return false;
        TestEqual(TEXT("Prepared follows Reservoir"),Node->Prerequisites[0].NodeId,FName(TEXT("Caster.Multispell.Reservoir")));
        FBreakerQuestFlagSet Flags;for(const auto& Mission:UBreakerMissionLibrary::GetMissions())for(const auto& Beat:Mission.Beats)
            for(const auto& Flag:UBreakerMissionLibrary::BeatCompletionFlags(Beat))Flags.Add(Flag);
        Progression->SettleDoctrineEntitlement(Flags);FText Reason;
        TestEqual(TEXT("Campaign pays eight doctrine points"),Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),8);
        TestFalse(TEXT("Wallet does not bypass investment gate"),Progression->PurchaseNode(Tree,Prepared,Reason));
        for(const TCHAR* Id:{TEXT("Caster.Multispell.Variance"),TEXT("Caster.Multispell.Cycle"),TEXT("Caster.Multispell.Reservoir"),TEXT("Caster.Multispell.Chain"),TEXT("Caster.Multispell.Payment"),TEXT("Caster.Multispell.Sequence")})
            if(!Progression->PurchaseNode(Tree,Id,Reason))return false;
        if(bBuyPrepared&&!Progression->PurchaseNode(Tree,Prepared,Reason))return false;
        TestEqual(TEXT("Exact optional two-point purchase"),Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),bBuyPrepared?0:2);
        const float ExpectedFloor=bBuyPrepared?-35.f:-20.f;
        TestEqual(TEXT("Published resource floor follows ownership"),Mana->GetPublishedFloor(),ExpectedFloor,.001f);
        TestEqual(TEXT("Authoritative attribute carries the same floor"),Attr->GetClassResourceFloor(),ExpectedFloor,.001f);
        TestEqual(TEXT("Prepared floor is editable authored tuning"),UBreakerManaComponent::GetResourceTuning().PreparedOvercastFloor,-35.f,.001f);
        auto* Abilities=Player->GetAbilities();const auto Melee=EBreakerAbilitySlot::ClassAbilityOne,Spell=EBreakerAbilitySlot::ClassAbilityTwo;
        if(!Progression->IsAbilityUnlocked(TEXT("Caster.Fracture"))&&!Progression->SpendAbilityToken(TEXT("Caster.Fracture"),Reason))return false;
        if(!Abilities->TryEquipAbility(Melee,TEXT("Caster.Cleave"),Reason)||!Abilities->TryEquipAbility(Spell,TEXT("Caster.Fracture"),Reason))return false;Abilities->RefreshGrants();
        Mana->AdvanceLoop(20);
        // Use the authoritative spend seam to establish a boundary bank; the
        // purchased Fracture crosses from a positive bank to -19. A cheaper
        // Cleave would need a negative starting bank, where casting is refused.
        // Keep the status-income boundary without bypassing that real refusal.
        const float SpellPrice=Abilities->GetResourceCostForSlot(Spell);
        if(!TestTrue(TEXT("Paid boundary setup remains above zero"),SpellPrice>19.f))return false;
        if(!Mana->TrySpendMana(Mana->GetMana()-(SpellPrice-19.f)))return false;
        TestEqual(TEXT("Cleave quotes ordinary price"),Abilities->GetResourceCostForSlot(Melee),12.f,.001f);
        if(!TestTrue(TEXT("Purchased spell enters the actual debt boundary"),Abilities->TryActivateSlot(Spell)))return false;
        TestEqual(TEXT("Native paid cast reaches status-income debt boundary"),Mana->GetMana(),-19.f,.001f);ASC->CancelAllAbilities();
        auto* Target=World->SpawnActor<AActor>();if(!Target)return false;
        auto* Sink=NewObject<UBreakerCombatComponent>(Target);Target->AddInstanceComponent(Sink);Sink->RegisterComponent();
        auto* Health=NewObject<UBreakerAttributeSet>(Target);Health->ApplyMaxHealth(1000);Health->ApplyHealth(1000);Sink->BindAttributes(Health);
        auto* Status=NewObject<UBreakerStatusComponent>(Target);Target->AddInstanceComponent(Status);Status->RegisterComponent();
        FBreakerDamageRequest ApplyingHit;ApplyingHit.BaseDamage=1;ApplyingHit.bCanCritical=false;ApplyingHit.SetInstigator(Player);
        const auto Landed=Sink->ReceiveDamage(ApplyingHit);if(!TestTrue(TEXT("Status is backed by native landed hit"),Landed.HealthDamage>0))return false;
        FBreakerStatusApplicationSpec Poison;Poison.StatusTag=FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"));Poison.BaseDamagePerTick=1;Poison.Duration=4;Poison.TickInterval=1;
        Status->ApplyStatusFromHit(Poison,EBreakerDamageFamily::Physical,ApplyingHit);
        if(!TestEqual(TEXT("Actual first status application accepted"),Status->GetDistinctStatusTypeCount(),1))return false;
        // This source-isolated interval preserves actual passive regeneration.
        // Variance rank1 queues4; .75s budget admits all4. Passive16.5 leaves
        // the bank negative before queue payment, so exactly8 status Mana pays.
        const float Before=Mana->GetMana();const float Seconds=.75f;
        const float Passive=Mana->PassiveRegenPerSecond*Seconds*Mana->OvercastGenerationMultiplier;
        Mana->AdvanceLoop(Seconds);
        TestEqual(TEXT("Overcast status income doubles exactly once with or without Prepared"),Mana->GetMana()-Before-Passive,8.f,.001f);
        Mana->AdvanceLoop(20);if(!Mana->TrySpendMana(Mana->GetMana()-4))return false;
        TestEqual(TEXT("Prepared never discounts the actual spell quote"),Abilities->GetResourceCostForSlot(Spell),30.f,.001f);
        TestEqual(TEXT("HUD affordability uses authored deeper floor"),Abilities->CanAffordSlot(Spell),bBuyPrepared);
        TestEqual(TEXT("Native activation agrees with floor affordability"),Abilities->TryActivateSlot(Spell),bBuyPrepared);
        TestEqual(TEXT("Refusal is free; accepted cast pays all thirty"),Mana->GetMana(),bBuyPrepared?-26.f:4.f,.001f);ASC->CancelAllAbilities();
        if(bBuyPrepared)
        {
            if(!Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints,true,Reason))return false;
            TestEqual(TEXT("Respec restores original floor"),Attr->GetClassResourceFloor(),-20.f,.001f);
            TestEqual(TEXT("Native floor clamp resolves old deeper debt"),Mana->GetMana(),-20.f,.001f);
            if(!Abilities->TryEquipAbility(Spell,TEXT("Caster.Fracture"),Reason))return false;Abilities->RefreshGrants();
            TestFalse(TEXT("Respec cannot retain deeper cast permission"),Abilities->TryActivateSlot(Spell));
        }
    }
    return true;
}
#endif
