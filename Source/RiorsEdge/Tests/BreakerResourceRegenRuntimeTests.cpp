#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerChargeComponent.h"
#include "Classes/BreakerGritComponent.h"
#include "Classes/BreakerManaComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Classes/BreakerScrapComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Classes/BreakerResourceGeneration.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerResourceRegenRuntimeTest,"RiorsEdge.Progression.ResourceRegenAndSecondShift",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerResourceRegenRuntimeTest::RunTest(const FString& Parameters)
{
    for (const auto ClassId : {EBreakerClassId::Caster,EBreakerClassId::Swift,EBreakerClassId::Tank,EBreakerClassId::Support,EBreakerClassId::Gunsmith})
    {
        UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
        if (!World) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        ON_SCOPE_EXIT {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
        World->InitializeActorsForPlay(FURL());
        auto* Player=World->SpawnActor<ABreakerCharacter>(); if (!Player) return false;
        auto* Attr=Player->GetAttributes(); auto* ASC=Player->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attr); Player->GetCombat()->BindAttributes(Attr);
        auto* Progression=Player->GetProgression(); Progression->BindAttributes(Attr);
        auto* Tree=NewObject<UBreakerProgressionTree>(); Tree->TreeId=TEXT("Test.Resource.Regen"); Tree->Currency=EBreakerPointCurrency::CorePoints;
        auto* Node=NewObject<UBreakerProgressionNode>(Tree); Node->NodeId=TEXT("Test.Resource.Regen.Rank"); Node->Currency=Tree->Currency;
        FBreakerNodeEffect Effect; Effect.StatTarget=EBreakerNodeStatTarget::ClassResourceRegen;
        Effect.StatBucket=EBreakerNodeStatBucket::Flat; Effect.ValuePerRank=1; Node->Effects.Add(Effect);
        Node->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.SecondShift"))); Tree->Nodes.Add(Node);
        auto* Class=NewObject<UBreakerClassDefinition>(); Class->ClassId=ClassId; Class->BranchTrees.Add(Tree);
        if (!TestTrue(TEXT("Native class selected"),Progression->ChoosePermanentClass(Class))) return false;
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(2,Progression->ExperienceCurve));
        auto* Mana=Player->GetMana(); auto* Momentum=Player->GetMomentum(); auto* Grit=Player->GetGrit();
        auto* Charge=Player->GetCharge(); auto* Scrap=Player->GetScrap();
        Mana->BindAttributes(Attr); Momentum->BindAttributes(Attr); Grit->BindAttributes(Attr); Charge->BindAttributes(Attr); Scrap->BindAttributes(Attr);
        Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        Player->GetBreakerMovement()->SetMovementMode(MOVE_Walking);
        for(int32 Frame=0;Frame<160;++Frame) World->Tick(LEVELTICK_All,.05f);
        TestFalse(TEXT("Actual combat age has expired"),Player->IsInResourceCombat());
        FText Reason;
        if(!TestTrue(TEXT("Earned point purchases primitive schema"),Progression->PurchaseNode(Tree,Node->NodeId,Reason))) return false;
        auto AdvanceAll=[&](float Seconds)
        {
            Mana->AdvanceLoop(Seconds); Momentum->AdvanceLoop(Seconds); Grit->AdvanceLoop(Seconds);
            Charge->AdvanceLoop(Seconds); Scrap->AdvanceLoop(Seconds);
        };
        // Bank setup isolates income/decay; this is not a campaign acquisition fixture.
        Attr->ApplyClassResource(50);
        const float NativeRegen=ClassId==EBreakerClassId::Caster ? Mana->PassiveRegenPerSecond : 0;
        AdvanceAll(.25f);
        TestEqual(FString::Printf(TEXT("Class %d: five components pay only one flat tick"),int32(ClassId)),
            Attr->GetClassResource(),50+(NativeRegen+1)*.25f,.001f);
        TestTrue(TEXT("Second Shift is active only outside actual combat"),BreakerResourceGeneration::HoldsOutOfCombatDecay(Player));
        if(ClassId==EBreakerClassId::Caster)
        {
            Mana->PushGenerationSuspension(TEXT("Test.Regen.Suspension")); const float Before=Attr->GetClassResource();
            AdvanceAll(.25f); TestEqual(TEXT("Mana suspension blocks both native and Core regen"),Attr->GetClassResource(),Before,.001f);
            Mana->PopGenerationSuspension(TEXT("Test.Regen.Suspension"));
        }
        FBreakerDamageRequest Hit; Hit.BaseDamage=1; Hit.DamageFamily=EBreakerDamageFamily::TrueDamage;
        Hit.bCanBeAvoided=false; Hit.bCanCritical=false; Hit.bBypassShield=true;
        Player->GetCombat()->ReceiveDamage(Hit);
        TestTrue(TEXT("Actual incoming hit opens combat state"),Player->IsInResourceCombat());
        TestFalse(TEXT("Second Shift does not suppress in-combat decay"),BreakerResourceGeneration::HoldsOutOfCombatDecay(Player));
        for(int32 Frame=0;Frame<160;++Frame) World->Tick(LEVELTICK_All,.05f);
        if(!TestTrue(TEXT("Actual respec removes both authored benefits"),Progression->RespecCore(Reason))) return false;
        TestEqual(TEXT("Flat rate removed"),BreakerResourceGeneration::FlatRate(Player),0.f);
        TestFalse(TEXT("Decay hold removed"),BreakerResourceGeneration::HoldsOutOfCombatDecay(Player));
        // Put Support above its authored OOC ceiling; 50 is already below the native60 clamp.
        const float DecayBank = ClassId==EBreakerClassId::Support ? Charge->OutOfCombatCeiling+10.0f : 50.0f;
        Attr->ApplyClassResource(DecayBank); AdvanceAll(8);
        if(ClassId==EBreakerClassId::Swift || ClassId==EBreakerClassId::Tank || ClassId==EBreakerClassId::Support)
            TestTrue(FString::Printf(TEXT("Class %d native out-of-combat decay returns after respec"),int32(ClassId)),Attr->GetClassResource()<DecayBank);
        if(!TestTrue(TEXT("Rebuy with refunded point"),Progression->PurchaseNode(Tree,Node->NodeId,Reason))) return false;
        Hit.BaseDamage=100000; Player->GetCombat()->ReceiveDamage(Hit);
        TestTrue(TEXT("Actual lethal hit kills owner"),Player->GetCombat()->IsDead());
        TestEqual(TEXT("Dead owner cannot earn new flat income"),BreakerResourceGeneration::FlatRate(Player),0.f);
    }
    return true;
}
#endif