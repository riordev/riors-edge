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
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::ClassResourceGeneration)==41);
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::SprintSpeed)==42);
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::Acceleration)==43);
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::JumpHeight)==44);
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::LedgeSpeed)==45);
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::SafeFallDistance)==46);
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::AirJumpCount)==47);
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerResourceGenerationRuntimeTest,"RiorsEdge.Progression.GenericResourceGeneration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerResourceGenerationRuntimeTest::RunTest(const FString& Parameters)
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
        // Primitive schema with earned purchase; no replacement Core roster or
        // extra point grants. Source notifications below isolate live loops.
        auto* Tree=NewObject<UBreakerProgressionTree>(); Tree->TreeId=TEXT("Test.Resource.Generation"); Tree->Currency=EBreakerPointCurrency::CorePoints;
        auto* Node=NewObject<UBreakerProgressionNode>(Tree); Node->NodeId=TEXT("Test.Resource.Generation.Rank"); Node->Currency=Tree->Currency;
        FBreakerNodeEffect Effect; Effect.StatTarget=EBreakerNodeStatTarget::ClassResourceGeneration;
        Effect.StatBucket=EBreakerNodeStatBucket::IncreasedPercent; Effect.ValuePerRank=50; Node->Effects.Add(Effect); Tree->Nodes.Add(Node);
        auto* Class=NewObject<UBreakerClassDefinition>(); Class->ClassId=ClassId; Class->BranchTrees.Add(Tree);
        if (!TestTrue(TEXT("Actual class selected"),Progression->ChoosePermanentClass(Class))) return false;
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(2,Progression->ExperienceCurve));
        auto* Mana=Player->GetMana(); auto* Momentum=Player->GetMomentum(); auto* Grit=Player->GetGrit();
        auto* Charge=Player->GetCharge(); auto* Scrap=Player->GetScrap();
        Mana->BindAttributes(Attr); Momentum->BindAttributes(Attr); Grit->BindAttributes(Attr); Charge->BindAttributes(Attr); Scrap->BindAttributes(Attr);
        auto* Movement=Player->GetBreakerMovement(); Movement->SetComponentTickEnabled(false); Movement->SetMovementMode(MOVE_Walking);
        if (ClassId==EBreakerClassId::Caster && !Mana->TrySpendMana(80)) return false;
        if (ClassId==EBreakerClassId::Tank) Grit->SetInCombat(true);
        if (ClassId==EBreakerClassId::Support) Charge->SetInCombat(true);
        auto Earn=[&]()
        {
            // Source ICDs use the world clock; small frames avoid WorldSettings hitch clamping.
            // Resource loops advance explicitly by the same full second below.
            for (int32 Frame = 0; Frame < 20; ++Frame) World->Tick(LEVELTICK_All, .05f);
            const float Before=Attr->GetClassResource();
            switch(ClassId)
            {
            case EBreakerClassId::Caster: Mana->AdvanceLoop(1); break;
            case EBreakerClassId::Swift:
                Movement->Velocity=FVector(Movement->WalkSpeed,0,0);
                Player->SetActorLocation(Player->GetActorLocation()+Movement->Velocity);
                Momentum->AdvanceLoop(1); break;
            case EBreakerClassId::Tank: Grit->NotifyMeleeKill(); Grit->AdvanceLoop(1); break;
            case EBreakerClassId::Support: Charge->NotifyStatusCleansed(1); Charge->AdvanceLoop(1); break;
            case EBreakerClassId::Gunsmith: Scrap->NotifyReloadCompleted(true); Scrap->AdvanceLoop(1); break;
            default: break;
            }
            return Attr->GetClassResource()-Before;
        };
        // Momentum first establishes its previous-location sample.
        if (ClassId==EBreakerClassId::Swift) Earn();
        const float Baseline=Earn();
        if (!TestTrue(FString::Printf(TEXT("Class %d normal income is positive"),static_cast<int32>(ClassId)),Baseline>0)) return false;
        FText Reason;
        if (!TestTrue(TEXT("Earned Core point buys generation"),Progression->PurchaseNode(Tree,Node->NodeId,Reason))) return false;
        TestEqual(TEXT("One shared Increased generation multiplier"),Progression->GetNodeStats().ClassResourceGenerationMultiplier,1.5f,.0001f);
        TestEqual(FString::Printf(TEXT("Class %d actual earned loop scales once"),static_cast<int32>(ClassId)),Earn(),Baseline*1.5f,.001f);
        const float BeforeDirect=Attr->GetClassResource();
        switch(ClassId)
        {
        case EBreakerClassId::Caster: Mana->GrantMana(1,true); break;
        case EBreakerClassId::Swift: Momentum->GrantMomentum(1); break;
        case EBreakerClassId::Tank: Grit->GrantGrit(1); break;
        case EBreakerClassId::Support: Charge->GrantCharge(1); break;
        case EBreakerClassId::Gunsmith: Scrap->GrantScrap(1); break;
        default: break;
        }
        TestEqual(TEXT("Explicit direct grant is not multiplied"),Attr->GetClassResource()-BeforeDirect,1.0f,.001f);
        if (ClassId==EBreakerClassId::Caster)
        {
            Mana->PushGenerationSuspension(TEXT("Test.ActualSuspension"));
            TestEqual(TEXT("Suspension still stops normal recovery"),Earn(),0.0f,.0001f);
            Mana->PopGenerationSuspension(TEXT("Test.ActualSuspension"));
        }
        if (ClassId==EBreakerClassId::Gunsmith)
        {
            // Source ICDs use the world clock; small frames avoid WorldSettings hitch clamping.
            // Resource loops advance explicitly by the same full second below.
            for (int32 Frame = 0; Frame < 20; ++Frame) World->Tick(LEVELTICK_All, .05f);
            const float Before=Attr->GetClassResource();
            const float Expected=Scrap->DestructionRefund(10,Scrap->GetEffectiveDestructionRefundFraction());
            Scrap->NotifyDeployableDestroyed(10); Scrap->AdvanceLoop(1);
            TestEqual(TEXT("Queued destruction refund is not multiplied"),Attr->GetClassResource()-Before,Expected,.001f);
        }
        if (!TestTrue(TEXT("Actual Core respec"),Progression->RespecCore(Reason))) return false;
        TestEqual(TEXT("Respec restores normal generation"),Earn(),Baseline,.001f);
    }
    return true;
}
#endif
