#include "Tests/BreakerReactionRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Tests/BreakerCastTestHelpers.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Resonance.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerElementReactions.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerExperience.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerInterferenceRuntimeTest,"RiorsEdge.Abilities.Caster.InterferenceRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerInterferenceRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Caster=World->SpawnActor<ABreakerCharacter>(FVector(0,0,200),FRotator::ZeroRotator,Spawn);
    auto* Target=World->SpawnActor<ABreakerCharacter>(FVector(800,0,200),FRotator::ZeroRotator,Spawn);
    if (!Caster||!Target) return false;
    for (auto* Player : {Caster,Target})
    {
        Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* ASC=Player->GetAbilitySystemComponent(); ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes()); Player->GetProgression()->BindAttributes(Player->GetAttributes());
    }
    Target->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel2,ECR_Block);
    // Durable isolated combat target: do not let a lethal burst erase the
    // preservation assertion; this changes no funding or count on the source.
    Target->GetAttributes()->ApplyMaxHealth(10000); Target->GetCombat()->RestoreVitals();
    auto* Progression=Caster->GetProgression();
    if (!Progression->ChoosePermanentClassById(EBreakerClassId::Caster)) return false;
    auto* Mana=Caster->GetMana(); Mana->BindAttributes(Caster->GetAttributes());
    auto* Abilities=Caster->GetAbilities(); auto* ASC=Caster->GetAbilitySystemComponent();
    const auto Slot=EBreakerAbilitySlot::ClassAbilityTwo;
    FText Reason;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(UBreakerProgressionLibrary::FirstAbilityTokenLevel,Progression->ExperienceCurve));
    if (!Progression->SpendAbilityToken(TEXT("Caster.Resonance"),Reason)||!Abilities->TryEquipAbility(Slot,TEXT("Caster.Resonance"),Reason)) return false;
    auto* Status=Target->FindComponentByClass<UBreakerStatusComponent>();
    auto* Observer=NewObject<UBreakerReactionRuntimeObserver>(Target);
    Target->GetCombat()->OnDamageTaken.AddDynamic(Observer,&UBreakerReactionRuntimeObserver::OnHit);
    const auto Rot=FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    const auto* Tree=UBreakerProgressionLibrary::GetCasterMultispellTree();
    const FName Interference(TEXT("Caster.Multispell.Interference"));
    const auto* Node=Tree->FindNode(Interference);
    if (!TestNotNull(TEXT("Shipped Interference node"),Node)) return false;
    // O272: Interference is the impactful half of Variance's pair.
    TestEqual(TEXT("Tier one"),Node->Tier,1); TestEqual(TEXT("One point"),Node->CostPerRank,1); TestEqual(TEXT("One rank"),Node->MaxRank,1);
    TestEqual(TEXT("No investment gate"),Node->RequiredTreeInvestment,0);
    TestFalse(TEXT("Ultimate cornerstone preserved"),Node->bCornerstone);
    auto CastCount=[&](int32 Count,bool bFixed,bool bPreserve)
    {
        Status->ConsumeAllStatuses(); Target->GetCombat()->RestoreVitals(); Mana->AdvanceLoop(30);
        // Two ordinary physical status payloads use the public application
        // API after an accepted hit. No private ActiveStatuses mutation, fake
        // elemental application, or retired sixth tag is used.
        FBreakerDamageRequest Physical; Physical.BaseDamage=1; Physical.bCanCritical=false; Physical.SetInstigator(Caster);
        const auto Accepted=Target->GetCombat()->ReceiveDamage(Physical);
        if (!TestTrue(TEXT("Physical fixture hit is accepted"),Accepted.HealthDamage+Accepted.ShieldDamage>0)) return false;
        for (const TCHAR* Name : {TEXT("Status.Bleed"),TEXT("Status.Poison")})
        {
            FBreakerStatusApplicationSpec Payload; Payload.StatusTag=FGameplayTag::RequestGameplayTag(Name);
            Payload.Duration=8; Payload.TickInterval=1; Payload.BaseDamagePerTick=1; Payload.InitialStacks=1;
            Status->ApplyStatusFromHit(Payload,EBreakerDamageFamily::Physical,Physical);
        }
        if (Count==3)
        {
            FBreakerDamageRequest Hit; Hit.BaseDamage=Status->GetEntropyThreshold()/(4*Caster->GetAttributes()->GetAbilityDamageMultiplier());
            Hit.Element=EBreakerElement::Entropy; Hit.ElementalFraction=1; Hit.DamageFamily=EBreakerDamageFamily::Elemental;
            Hit.bCanCritical=false; Hit.SetInstigator(Caster); UBreakerDamageLibrary::FillSourcePools(Caster->GetAttributes(),EBreakerDamageDelivery::Ability,Hit);
            for(int32 I=0;I<20&&!Status->HasStatus(Rot);++I) Target->GetCombat()->ReceiveDamage(Hit);
        }
        if (!TestEqual(TEXT("Only actual requested statuses present"),Status->GetDistinctStatusTypeCount(),Count)) return false;
        const auto BeforeStatuses=Status->GetActiveStatuses();
        Observer->Hits.Reset();
        const float SourceMultiplier=Caster->GetAttributes()->GetAbilityDamageMultiplier();
        const float CriticalMultiplier=Caster->GetAttributes()->GetCriticalMultiplier();
        const float AbilityScalar=UBreakerGameplayAbility::AbilityDamageScalarFor(Caster);
        const auto* BeforeSpec=ASC->FindAbilitySpecFromClass(UBreakerAbility_Resonance::StaticClass());
        const auto* BeforeInstance=BeforeSpec ? Cast<UBreakerAbility_Resonance>(BeforeSpec->GetPrimaryInstance()) : nullptr;
        if (!BeforeInstance) return false;
        const float CurveDamage=UBreakerStatusConsumption::DetonationDamage(Count,BeforeInstance->Detonation,bFixed?EBreakerDetonationCurve::FixedPlusThreshold:BeforeInstance->Curve);
        const float Expected=UBreakerGameplayAbility::AbilityBaseDamageFor(Caster,CurveDamage*AbilityScalar)*SourceMultiplier;
        const float ManaBefore=Mana->GetMana(); const float Quote=Abilities->GetResourceCostForSlot(Slot);
        if (!TestTrue(TEXT("Normally equipped paid Resonance activates"),Abilities->TryActivateSlot(Slot))) return false;
        BreakerResolvePendingCast(World, Caster);
        const auto* Spec=ASC->FindAbilitySpecFromClass(UBreakerAbility_Resonance::StaticClass());
        const auto* Instance=Spec ? Cast<UBreakerAbility_Resonance>(Spec->GetPrimaryInstance()) : nullptr;
        if (!Instance||!TestEqual(TEXT("One authored burst, no reaction recursion"),Observer->Hits.Num(),1)) return false;

        TestEqual(TEXT("Actual burst uses selected authored curve"),Observer->Hits[0].Result.RawDamage,Expected*(Observer->Hits[0].Result.bCritical ? CriticalMultiplier : 1.0f),.01f);
        TestEqual(TEXT("Displayed quote is authoritative debit"),Instance->GetLastPaidResourceCost(),Quote,.001f);
        const float Refund=bPreserve ? Count*Instance->PaymentRankOneManaPerStatus : 0;
        TestEqual(TEXT("Existing Payment refund remains separate from debit"),Mana->GetMana(),ManaBefore-Quote+Refund,.001f);
        if (bPreserve)
        {
            TestEqual(TEXT("Existing Resonance preservation still works"),Status->GetDistinctStatusTypeCount(),Count);
            for(const auto& Before : BeforeStatuses)
                for(const auto& After : Status->GetActiveStatuses())
                    if(Before.ApplicationSerial==After.ApplicationSerial)
                    {
                        TestEqual(TEXT("Existing remaining lifetime is halved once"),After.RemainingDuration,Before.RemainingDuration*.5f,.001f);
                        if (Before.Spec.StatusTag==Rot)
                            TestEqual(TEXT("Rot scheduled unpaid budget halves with lifetime"),BreakerElementReactions::RemainingRotBudget(After),BreakerElementReactions::RemainingRotBudget(Before)*.5f,.001f);
                    }
        }
        else TestTrue(TEXT("Ordinary Resonance consumes statuses"),Status->GetActiveStatuses().IsEmpty());
        return true;
    };
    if (!CastCount(2,false,false)||!CastCount(3,false,false)) return false;
    TestFalse(TEXT("No campaign entitlement cannot buy node"),Progression->PurchaseNode(Tree,Interference,Reason));
    FBreakerQuestFlagSet Flags;
    for(const auto& Mission:UBreakerMissionLibrary::GetMissions()) for(const auto& Beat:Mission.Beats)
        for(const auto& Flag:UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
    Progression->SettleDoctrineEntitlement(Flags);
    TestEqual(TEXT("Actual campaign entitlement is eight"),Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),8);
    TestFalse(TEXT("The travel prerequisite is still required"),Progression->PurchaseNode(Tree,Interference,Reason));
    // Two pairs (O272): Payment -> Resonance is what the preservation and
    // refund assertions below compose with; Variance -> Interference is the
    // node under test. Four points of the eight.
    for(const TCHAR* Id:{TEXT("Caster.Multispell.Payment"),TEXT("Caster.Multispell.Resonance"),TEXT("Caster.Multispell.Variance")})
        if(!Progression->PurchaseNode(Tree,Id,Reason)) return false;
    if(!Progression->PurchaseNode(Tree,Interference,Reason)) return false;
    TestEqual(TEXT("Two pairs cost four earned points"),Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),4);
    if(!CastCount(2,true,true)||!CastCount(3,true,true)) return false;
    // Six distinct live statuses are not currently reachable: five registered
    // types include immediately-paid Unstable. Keep the authored future cap
    // and shape check numerical rather than claiming fabricated runtime six.
    const auto& Params=GetDefault<UBreakerAbility_Resonance>()->Detonation;
    const float FixedRatio=UBreakerStatusConsumption::DetonationRatio(2,6,Params,EBreakerDetonationCurve::FixedPlusThreshold);
    TestTrue(TEXT("Interference remains flatter through six-count cap"),FixedRatio<UBreakerStatusConsumption::DetonationRatio(2,6,Params,EBreakerDetonationCurve::Linear));
    TestTrue(TEXT("Six-count bound remains2.2"),FixedRatio<=2.2f);
    if(!Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints,true,Reason)) return false;
    if(!Abilities->TryEquipAbility(Slot,TEXT("Caster.Resonance"),Reason)) return false;
    if(!CastCount(2,false,false)||!CastCount(3,false,false)) return false;
    return true;
}
#endif