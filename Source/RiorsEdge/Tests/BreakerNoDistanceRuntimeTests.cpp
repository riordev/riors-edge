#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Closequarter.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Progression/BreakerExperience.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionNode.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerNoDistanceRuntimeTest,"RiorsEdge.Abilities.Caster.NoDistanceRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerNoDistanceRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FVector Origin(0,0,200);
    auto* Caster=World->SpawnActor<ABreakerCharacter>(Origin,FRotator::ZeroRotator,Spawn);
    auto* Target=World->SpawnActor<ABreakerCharacter>(Origin+FVector(800,0,0),FRotator::ZeroRotator,Spawn);
    if (!TestNotNull(TEXT("Caster spawned"),Caster)||!TestNotNull(TEXT("Target spawned"),Target)) return false;
    for (auto* Player : {Caster,Target})
    {
        Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* ASC=Player->GetAbilitySystemComponent(); ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes()); Player->GetProgression()->BindAttributes(Player->GetAttributes());
    }
    Target->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel2,ECR_Block);
    auto* Progression=Caster->GetProgression();
    if (!Progression->ChoosePermanentClassById(EBreakerClassId::Caster)) return false;
    auto* Mana=Caster->GetMana(); Mana->BindAttributes(Caster->GetAttributes());
    auto* ASC=Caster->GetAbilitySystemComponent();
    FText Reason;
    auto* Abilities=Caster->GetAbilities();
    const auto Slot=EBreakerAbilitySlot::ClassAbilityTwo;
    const FName ClosequarterId(TEXT("Caster.Closequarter"));
    TestFalse(TEXT("Locked Closequarter cannot be equipped"),Abilities->TryEquipAbility(Slot,ClosequarterId,Reason));
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(UBreakerProgressionLibrary::FirstAbilityTokenLevel,Progression->ExperienceCurve));
    if (!TestTrue(TEXT("Earned token unlocks Closequarter"),Progression->SpendAbilityToken(ClosequarterId,Reason))) return false;
    // Only component initialization is omitted to avoid owner save loading.
    // The actual equip route grants GAS through RefreshGrants, not GiveAbility.
    if (!TestTrue(TEXT("Unlocked Closequarter equips through normal component"),Abilities->TryEquipAbility(Slot,ClosequarterId,Reason))) return false;
    if (!TestTrue(TEXT("Equipped slot is actually granted"),Abilities->IsSlotGranted(Slot))) return false;
    auto CastClosequarter=[&](float ExpectedPaid,float ExpectedRefund)
    {
        Caster->SetActorLocation(Origin,false); Mana->AdvanceLoop(30);
        const float Before=Mana->GetMana();
        TestEqual(TEXT("Equipped HUD quote matches paid price"),Abilities->GetResourceCostForSlot(Slot),ExpectedPaid,.001f);
        if (!TestTrue(TEXT("Native paid Closequarter activates"),Abilities->TryActivateSlot(Slot))) return false;
        const auto* Spec=ASC->FindAbilitySpecFromClass(UBreakerAbility_Closequarter::StaticClass());
        const auto* Instance=Spec ? Cast<UBreakerAbility_Closequarter>(Spec->GetPrimaryInstance()) : nullptr;
        if (!TestNotNull(TEXT("Actual ability instance"),Instance)) return false;
        TestEqual(TEXT("Authoritative committed quote is debited"),Instance->GetLastPaidResourceCost(),ExpectedPaid,.001f);
        TestEqual(TEXT("Arrival refund follows target health rule"),Mana->GetMana(),Before-ExpectedPaid+ExpectedRefund,.001f);
        TestTrue(TEXT("Native blink reaches toward target"),Caster->GetActorLocation().X>Origin.X+100);
        return true;
    };
    if (!CastClosequarter(35,0)) return false;
    Target->GetAttributes()->ApplyHealth(Target->GetAttributes()->GetMaxHealth()*.4f);
    if (!CastClosequarter(35,15)) return false;
    Target->GetCombat()->RestoreVitals();
    const auto* Tree=UBreakerProgressionLibrary::GetCasterSpellbladeTree();
    const FName NodeId(TEXT("Caster.Spellblade.NoDistance"));
    const auto* Node=Tree->FindNode(NodeId);
    if (!TestNotNull(TEXT("Shipped No Distance exists"),Node)) return false;
    // O272: No Distance is the impactful half of Close's pair.
    TestEqual(TEXT("Tier one"),Node->Tier,1); TestEqual(TEXT("One point"),Node->CostPerRank,1); TestEqual(TEXT("One rank"),Node->MaxRank,1);
    TestEqual(TEXT("No investment gate"),Node->RequiredTreeInvestment,0);
    TestFalse(TEXT("Does not replace cornerstone"),Node->bCornerstone);
    TestFalse(TEXT("Cannot buy before campaign entitlement"),Progression->PurchaseNode(Tree,NodeId,Reason));
    FBreakerQuestFlagSet Flags;
    for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
        for (const auto& Beat : Mission.Beats)
            for (const auto& Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
    Progression->SettleDoctrineEntitlement(Flags);
    TestEqual(TEXT("Authored campaign grants eight points"),Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),8);
    TestFalse(TEXT("Wallet alone cannot bypass the travel prerequisite"),Progression->PurchaseNode(Tree,NodeId,Reason));
    if (!TestTrue(TEXT("Campaign-funded travel purchase"),Progression->PurchaseNode(Tree,TEXT("Caster.Spellblade.Close"),Reason))) return false;
    if (!TestTrue(TEXT("The pair fits one benchmark's two points"),Progression->PurchaseNode(Tree,NodeId,Reason))) return false;
    TestEqual(TEXT("The pair spends two of the eight"),Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),6);
    if (!CastClosequarter(50,15)) return false;
    Caster->SetActorLocation(Origin,false);
    const float Floor=Caster->GetAttributes()->GetClassResourceFloor();
    const float Insufficient=Floor+49;
    if (!Mana->TrySpendMana(Mana->GetMana()-Insufficient)) return false;
    const float BeforeRefusal=Mana->GetMana();
    TestFalse(TEXT("Cost fifty respects actual negative floor"),Abilities->TryActivateSlot(Slot));
    TestEqual(TEXT("Refused cast has no debit or refund"),Mana->GetMana(),BeforeRefusal);
    TestEqual(TEXT("Refused cast does not move"),Caster->GetActorLocation(),Origin);
    if (!Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints,true,Reason)) return false;
    if (!TestTrue(TEXT("Normal re-equip after Doctrine respec restores starter loadout"),
        Abilities->TryEquipAbility(Slot, ClosequarterId, Reason))) return false;
    if (!CastClosequarter(35,0)) return false;
    return true;
}
#endif
