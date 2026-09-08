#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Abilities/BreakerGunsmithAbilities.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Classes/BreakerScrapComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerExperience.h"
#include "Weapons/BreakerWeaponComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerScopedDurationRuntimeTest,"RiorsEdge.Abilities.CoreScopedDurationRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerScopedDurationRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter=Frame; };
    auto Clock=[&](float Seconds) { for(float T=0;T<Seconds;T+=.05f) {++GFrameCounter; World->Tick(LEVELTICK_All,.05f);} };
    for (const auto ClassId : {EBreakerClassId::Caster,EBreakerClassId::Gunsmith})
    {
        const FVector Position(ClassId==EBreakerClassId::Caster?0:20000,0,200);
        auto* Player=World->SpawnActor<ABreakerCharacter>(Position,FRotator::ZeroRotator);
        if(!Player) return false;
        Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* ASC=Player->GetAbilitySystemComponent(); auto* Attributes=Player->GetAttributes();
        ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Attributes);
        Player->GetCombat()->BindAttributes(Attributes); auto* Progression=Player->GetProgression(); Progression->BindAttributes(Attributes);
        // Temporary schema exercises authored upcoming prices without enabling
        // an incomplete replacement Core roster. Class kit remains shipped.
        auto* Class=DuplicateObject<UBreakerClassDefinition>(UBreakerProgressionLibrary::GetFallbackClassDefinition(ClassId),Player);
        auto* Tree=NewObject<UBreakerProgressionTree>(Class); Tree->TreeId=TEXT("Test.Core.ScopedDuration"); Tree->Currency=EBreakerPointCurrency::CorePoints;
        Class->BranchTrees.Add(Tree);
        auto Add=[&](const TCHAR* Id,EBreakerNodeStatTarget Target,float Percent,int32 Cost)
        {
            auto* Node=NewObject<UBreakerProgressionNode>(Tree); Node->NodeId=Id; Node->Currency=Tree->Currency; Node->CostPerRank=Cost; Node->MaxRank=1;
            FBreakerNodeEffect Effect; Effect.StatTarget=Target; Effect.StatBucket=EBreakerNodeStatBucket::IncreasedPercent; Effect.ValuePerRank=Percent;
            Node->Effects.Add(Effect); Tree->Nodes.Add(Node); return Node;
        };
        auto* Hold=Add(TEXT("Test.Duration.Hold"),EBreakerNodeStatTarget::AbilityDuration,10,1);
        auto* Persistence=Add(TEXT("Test.Duration.Persistence"),EBreakerNodeStatTarget::ZoneAndWindowDuration,20,2);
        auto* Uptime=Add(TEXT("Test.Duration.Uptime"),EBreakerNodeStatTarget::BuffAndWindowDuration,15,2);
        if(!Progression->ChoosePermanentClass(Class)) return false;
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(6,Progression->ExperienceCurve));
        FText Reason;
        if(!Progression->PurchaseNode(Tree,Hold->NodeId,Reason)) return false;
        TestEqual(TEXT("Hold independently scales generic durations"),UBreakerGameplayAbility::AbilityDurationMultiplierFor(Player),1.10f,.001f);
        if(!Progression->PurchaseNode(Tree,Persistence->NodeId,Reason)) return false;
        TestEqual(TEXT("Persistence joins generic for zones"),UBreakerGameplayAbility::AbilityDurationMultiplierFor(Player,EBreakerAbilityDurationKind::Zone),1.30f,.001f);
        TestEqual(TEXT("Persistence does not extend buff-only durations"),UBreakerGameplayAbility::AbilityDurationMultiplierFor(Player,EBreakerAbilityDurationKind::Buff),1.10f,.001f);
        if(!Progression->PurchaseNode(Tree,Uptime->NodeId,Reason)) return false;
        TestEqual(TEXT("All scoped contributions add once for window"),UBreakerGameplayAbility::AbilityDurationMultiplierFor(Player,EBreakerAbilityDurationKind::Window),1.45f,.001f);
        TestEqual(TEXT("Buff-only helper is generic plus Uptime"),UBreakerGameplayAbility::AbilityDurationMultiplierFor(Player,EBreakerAbilityDurationKind::Buff),1.25f,.001f);
        TestEqual(TEXT("Uptime does not extend zones"),UBreakerGameplayAbility::AbilityDurationMultiplierFor(Player,EBreakerAbilityDurationKind::Zone),1.30f,.001f);
        TestEqual(TEXT("Scope modifiers do not leak into generic"),UBreakerGameplayAbility::AbilityDurationMultiplierFor(Player),1.10f,.001f);
        auto* Abilities=Player->GetAbilities(); const auto Slot=EBreakerAbilitySlot::ClassAbilityTwo;
        const FName AbilityId(ClassId==EBreakerClassId::Caster ? TEXT("Caster.Rot") : TEXT("Gunsmith.Overhaul"));
        if(!Progression->IsAbilityUnlocked(AbilityId) && !Progression->SpendAbilityToken(AbilityId,Reason)) return false;
        if(!Abilities->TryEquipAbility(Slot,AbilityId,Reason)) return false;
        Abilities->RefreshGrants();
        Player->GetWeapon()->ResetAmmunition();
        if(ClassId==EBreakerClassId::Caster) {Player->GetMana()->BindAttributes(Attributes); Player->GetMana()->AdvanceLoop(30);}
        else Player->FindComponentByClass<UBreakerScrapComponent>()->BindAttributes(Attributes);
        // This isolated world omits Character BeginPlay to avoid owner saves.
        // Register and begin only the native window clock used by this test.
        auto* WindowState=UBreakerAbilityStateComponent::FindOrAdd(Player);
        WindowState->RegisterAllComponentTickFunctions(true); WindowState->SetComponentTickEnabled(true);
        if (!WindowState->HasBegunPlay()) WindowState->BeginPlay();
        const float ResourceBefore=Attributes->GetClassResource();
        const float Quote=Abilities->GetResourceCostForSlot(Slot);
        if(!TestTrue(FString::Printf(TEXT("Normal equipped %s activates"), *AbilityId.ToString()),Abilities->TryActivateSlot(Slot))) return false;
        if(ClassId==EBreakerClassId::Caster)
        {
            TestEqual(TEXT("Rot pays its live Mana quote"),Attributes->GetClassResource(),ResourceBefore-Quote,.001f);
            ABreakerZoneActor* Zone=nullptr;
            for(TActorIterator<ABreakerZoneActor> It(World);It;++It) if(It->GetZoneInstigator()==Player&&!It->IsReleased()) {Zone=*It;break;}
            if(!TestNotNull(TEXT("Native Rot emitted real zone"),Zone)) return false;
            // ConfigureZone stores the budget; BeginPlay plus explicit actor
            // tick registration gives that budget its real native lifetime.
            if (!Zone->HasActorBegunPlay()) Zone->DispatchBeginPlay();
            Zone->RegisterAllActorTickFunctions(true,true); Zone->SetActorTickEnabled(true);
            const float Expected=GetDefault<UBreakerAbility_Rot>()->DurationSeconds*1.30f;
            TestEqual(TEXT("Emitted zone uses additive scoped lifetime"),Zone->GetRemainingDuration(),Expected,.001f);
            if(!Progression->RespecCore(Reason)) return false;
            TestEqual(TEXT("Respec does not rewrite emitted lifetime"),Zone->GetRemainingDuration(),Expected,.001f);
            Clock(GetDefault<UBreakerAbility_Rot>()->DurationSeconds+.1f);
            TestFalse(TEXT("Zone survives original generic-free expiry"),Zone->IsReleased());
            Clock(Expected-GetDefault<UBreakerAbility_Rot>()->DurationSeconds+.1f);
            TestTrue(TEXT("Zone naturally ends after scoped lifetime"),!IsValid(Zone)||Zone->IsReleased());
        }
        else
        {
            auto* State=Player->FindComponentByClass<UBreakerAbilityStateComponent>();
            if(!TestNotNull(TEXT("Overhaul publishes real HUD window"),State)) return false;
            const float Base=UBreakerAbilityDefinition::FindFallback(AbilityId)->WindowDuration;
            const float Expected=Base*1.45f;
            TestEqual(TEXT("HUD window uses scoped lifetime"),State->GetWindowRemaining(UBreakerAbility_Overhaul::WindowKey()),Expected,.001f);
            const auto* Spec=ASC->FindAbilitySpecFromClass(UBreakerAbility_Overhaul::StaticClass());
            if(!TestTrue(TEXT("Overhaul gameplay remains active"),Spec&&Spec->IsActive())) return false;
            if(!Progression->RespecCore(Reason)) return false;
            Clock(Base+.1f);
            Spec=ASC->FindAbilitySpecFromClass(UBreakerAbility_Overhaul::StaticClass());
            TestTrue(TEXT("Gameplay timer survives old base expiry after respec"),Spec&&Spec->IsActive());
            TestTrue(TEXT("HUD and gameplay agree before scoped expiry"),State->IsWindowActive(UBreakerAbility_Overhaul::WindowKey()));
            Clock(Expected-Base+.1f);
            Spec=ASC->FindAbilitySpecFromClass(UBreakerAbility_Overhaul::StaticClass());
            TestTrue(TEXT("Gameplay timer ends scoped window"),Spec&&!Spec->IsActive());
            TestFalse(TEXT("HUD ends with gameplay timer"),State->IsWindowActive(UBreakerAbility_Overhaul::WindowKey()));
        }
    }
    FBreakerNodeStats Negative;
    Negative.AbilityDurationPercent=-150; Negative.ZoneAndWindowDurationPercent=20;
    TestEqual(TEXT("Floor is applied after additive composition"),UBreakerGameplayAbility::ComposeAbilityDurationMultiplier(Negative,EBreakerAbilityDurationKind::Zone),0.f);
    auto* Bad=NewObject<UBreakerProgressionNode>(); FBreakerNodeEffect BadEffect;
    BadEffect.StatTarget=EBreakerNodeStatTarget::ZoneAndWindowDuration; BadEffect.StatBucket=EBreakerNodeStatBucket::MorePercent;
    Bad->Effects.Add(BadEffect);
    TestFalse(TEXT("Scoped duration refuses unsupported More authoring"),UBreakerProgressionComponent::IsNodeMoreAuthoringLegal(Bad));
    return true;
}
#endif
