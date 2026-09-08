#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionNode.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerReprisalRuntimeTest,"RiorsEdge.Abilities.Caster.ReprisalRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerReprisalRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT {World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter=Frame;};
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Player=World->SpawnActor<ABreakerCharacter>(FVector(0,0,200),FRotator::ZeroRotator,Spawn);
    auto* Target=World->SpawnActor<ABreakerCharacter>(FVector(200,0,200),FRotator(0,180,0),Spawn);
    if(!Player||!Target) return false;
    for(auto* Character:{Player,Target})
    {
        Character->SetActorTickEnabled(false); Character->GetBreakerMovement()->SetComponentTickEnabled(false);
        auto* ASC=Character->GetAbilitySystemComponent(); ASC->InitAbilityActorInfo(Character,Character); ASC->AddAttributeSetSubobject(Character->GetAttributes());
        Character->GetCombat()->BindAttributes(Character->GetAttributes()); Character->GetProgression()->BindAttributes(Character->GetAttributes());
    }
    Target->GetAttributes()->ApplyMaxHealth(10000); Target->GetCombat()->RestoreVitals();
    auto* Progression=Player->GetProgression(); if(!Progression->ChoosePermanentClassById(EBreakerClassId::Caster)) return false;
    auto* Mana=Player->GetMana(); Mana->BindAttributes(Player->GetAttributes()); Mana->AdvanceLoop(30);
    auto* Combat=Player->GetCombat(); auto* ASC=Player->GetAbilitySystemComponent(); auto* Abilities=Player->GetAbilities();
    auto* State=UBreakerAbilityStateComponent::FindOrAdd(Player);
    State->RegisterAllComponentTickFunctions(true);State->SetComponentTickEnabled(true);
    if(!State->HasBegunPlay())State->BeginPlay();
    const auto Slot=EBreakerAbilitySlot::ClassAbilityOne; FText Reason;
    if(!Abilities->TryEquipAbility(Slot,TEXT("Caster.Cleave"),Reason)) return false;
    Abilities->RefreshGrants();
    auto Clock=[&](float Seconds){for(float T=0;T<Seconds;T+=.01f){++GFrameCounter;World->Tick(LEVELTICK_All,.01f);}};
    auto CastCleave=[&](float Price)
    {
        const float Before=Mana->GetMana(); const float VictimBefore=Target->GetAttributes()->GetHealth();
        TestEqual(TEXT("HUD quote is live"),Abilities->GetResourceCostForSlot(Slot),Price,.001f);
        if(!TestTrue(TEXT("Native Cleave activates"),Abilities->TryActivateSlot(Slot))) return false;
        const auto* Spec=ASC->FindAbilitySpecFromClass(UBreakerAbility_Cleave::StaticClass());
        const auto* Ability=Spec?Cast<UBreakerAbility_Cleave>(Spec->GetPrimaryInstance()):nullptr;
        if(!Ability) return false;
        TestEqual(TEXT("Committed debit matches quoted price"),Ability->GetLastPaidResourceCost(),Price,.001f);
        TestEqual(TEXT("Actual bank pays price exactly once"),Mana->GetMana(),Before-Price,.001f);
        TestTrue(TEXT("Cleave still lands real melee damage"),Target->GetAttributes()->GetHealth()<VictimBefore);
        return true;
    };
    if(!CastCleave(20)) return false; Clock(.5f);
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5,Progression->ExperienceCurve));
    if(!Progression->PurchaseNode(UBreakerProgressionLibrary::GetCoreSliceTree(),TEXT("Core.Bulwark.Read"),Reason)) return false;
    int32 Seed=1; int32 LastPassiveBlockSeed=0;
    auto EarnPassiveBlock=[&]()
    {
        for(int32 I=0;I<512;++I)
        {
            FBreakerDamageRequest Hit; Hit.BaseDamage=.05f; Hit.bCanCritical=false; Hit.SetInstigator(Target); Hit.RandomSeed=Seed++;
            Hit.SourceLocation=Target->GetActorLocation(); Hit.bHasSourceLocation=true;
            const auto Result=Combat->ReceiveDamage(Hit);
            if(Result.bBlocked) {LastPassiveBlockSeed=Hit.RandomSeed; return TestFalse(TEXT("Passive roll is not a parry"),Result.bParried);}
        }
        AddError(TEXT("Earned six-percent passive block did not occur in bounded native hits")); return false;
    };
    if(!EarnPassiveBlock()) return false;
    TestFalse(TEXT("Unowned Reprisal does not arm from block"),Combat->HasReprisalCharge());
    const auto* Tree=UBreakerProgressionLibrary::GetCasterSpellbladeTree(); const FName Reprisal(TEXT("Caster.Spellblade.Reprisal"));
    const auto* Node=Tree->FindNode(Reprisal); if(!TestNotNull(TEXT("Authored Reprisal node"),Node)) return false;
    TestEqual(TEXT("Tier four"),Node->Tier,4);TestEqual(TEXT("One rank"),Node->MaxRank,1);TestEqual(TEXT("Two points"),Node->CostPerRank,2);
    TestEqual(TEXT("Recovered tier-four path has one prerequisite"),Node->Prerequisites.Num(),1);
    if(Node->Prerequisites.Num()!=1)return false;
    TestEqual(TEXT("Historical Reprisal follows Bloodprice"),Node->Prerequisites[0].NodeId,FName(TEXT("Caster.Spellblade.Bloodprice")));
    TestEqual(TEXT("One Bloodprice rank opens the path"),Node->Prerequisites[0].RequiredRank,1);
    TestFalse(TEXT("Entitlement needed"),Progression->PurchaseNode(Tree,Reprisal,Reason));
    FBreakerQuestFlagSet Flags;
    for(const auto& Mission:UBreakerMissionLibrary::GetMissions())for(const auto& Beat:Mission.Beats)
        for(const auto& Flag:UBreakerMissionLibrary::BeatCompletionFlags(Beat))Flags.Add(Flag);
    Progression->SettleDoctrineEntitlement(Flags);
    TestEqual(TEXT("Shipped campaign pays eight"),Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),8);
    auto BuyReprisal=[&]()
    {
        for(const TCHAR* Id:{TEXT("Caster.Spellblade.ContactCharge"),TEXT("Caster.Spellblade.FollowThrough"),TEXT("Caster.Spellblade.Close"),TEXT("Caster.Spellblade.Debt"),TEXT("Caster.Spellblade.MomentumTransfer"),TEXT("Caster.Spellblade.Bloodprice")})
            if(!Progression->PurchaseNode(Tree,Id,Reason))return false;
        return Progression->PurchaseNode(Tree,Reprisal,Reason);
    };
    TestFalse(TEXT("Eight-point wallet cannot bypass investment"),Progression->PurchaseNode(Tree,Reprisal,Reason));
    if(!BuyReprisal())return false;
    TestEqual(TEXT("Legitimate route spends eight"),Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),0);
    if(!EarnPassiveBlock())return false;
    TestTrue(TEXT("Actual passive block arms one free Cleave"),Combat->HasReprisalCharge());
    if(!Mana->TrySpendMana(Mana->GetMana()-Player->GetAttributes()->GetClassResourceFloor()))return false;
    if(!CastCleave(0))return false;
    TestFalse(TEXT("Successful commit claims charge"),Combat->HasReprisalCharge());
    if(!EarnPassiveBlock())return false;
    TestFalse(TEXT("Existing swing lock refuses immediate second cast"),Abilities->TryActivateSlot(Slot));
    TestTrue(TEXT("Refused cast preserves still-live charge"),Combat->HasReprisalCharge());
    Clock(.5f);
    if(!CastCleave(0))return false;
    Clock(.5f);
    TestEqual(TEXT("Spent opportunity restores ordinary quote"),Abilities->GetResourceCostForSlot(Slot),20.f,.001f);
    if(!EarnPassiveBlock())return false;
    Clock(GetDefault<UBreakerAbility_Cleave>()->ReprisalWindowSeconds+.05f);
    TestFalse(TEXT("Unused opportunity expires on authored clock"),Combat->HasReprisalCharge());
    Mana->AdvanceLoop(30);
    if(!CastCleave(20))return false; Clock(.5f);
    if(!EarnPassiveBlock())return false;
    if(!Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints,true,Reason))return false;
    TestFalse(TEXT("Respec invalidates earned opportunity"),Combat->HasReprisalCharge());
    if(!BuyReprisal()||!Abilities->TryEquipAbility(Slot,TEXT("Caster.Cleave"),Reason))return false;
    TestFalse(TEXT("Repurchase cannot resurrect old opportunity"),Combat->HasReprisalCharge());
    if(!EarnPassiveBlock())return false;
    FBreakerDamageRequest Lethal;Lethal.BaseDamage=100000;Lethal.bCanCritical=false;Lethal.bCanBeAvoided=false;Lethal.SetInstigator(Target);
    Combat->ReceiveDamage(Lethal);Combat->RestoreVitals();
    TestFalse(TEXT("Death and revive cannot resurrect opportunity"),Combat->HasReprisalCharge());
    // Replay a seed earned by an actual passive block, with the same defense.
    // No forced block chance: a lethal blocked hit must not create a charge.
    if(!EarnPassiveBlock())return false;
    Clock(GetDefault<UBreakerAbility_Cleave>()->ReprisalWindowSeconds+.05f);
    TestFalse(TEXT("Lethal-block probe starts without an opportunity"),Combat->HasReprisalCharge());
    FBreakerDamageRequest BlockedLethal;
    BlockedLethal.BaseDamage=100000;BlockedLethal.bCanCritical=false;
    BlockedLethal.SetInstigator(Target);BlockedLethal.RandomSeed=LastPassiveBlockSeed;
    BlockedLethal.SourceLocation=Target->GetActorLocation();BlockedLethal.bHasSourceLocation=true;
    const auto BlockedDeath=Combat->ReceiveDamage(BlockedLethal);
    TestTrue(TEXT("Earned deterministic seed resolves a native passive block"),BlockedDeath.bBlocked);
    TestTrue(TEXT("Blocked lethal damage still kills"),BlockedDeath.bKilled);
    Combat->RestoreVitals();
    TestFalse(TEXT("Lethal passive block cannot arm a revived player"),Combat->HasReprisalCharge());
    // The outer small hit genuinely blocks. Its native health notification
    // kills the owner before the outer hit reaches Reprisal's arming site.
    // This is not OnDeath dispatch or a forced block chance.
    if(!EarnPassiveBlock())return false;
    Clock(GetDefault<UBreakerAbility_Cleave>()->ReprisalWindowSeconds+.05f);
    bool bEnteredNestedDamage=false;
    bool bNestedKilled=false;
    const FDelegateHandle HealthCallback=ASC->GetGameplayAttributeValueChangeDelegate(UBreakerAttributeSet::GetHealthAttribute()).AddLambda(
        [&](const FOnAttributeChangeData& Change)
        {
            if(bEnteredNestedDamage || Change.NewValue>=Change.OldValue)return;
            bEnteredNestedDamage=true;
            FBreakerDamageRequest Nested;
            Nested.BaseDamage=100000;Nested.bCanCritical=false;Nested.bCanBeAvoided=false;
            Nested.DamageFamily=EBreakerDamageFamily::TrueDamage;Nested.SetInstigator(Target);
            bNestedKilled=Combat->ReceiveDamage(Nested).bKilled;
        });
    FBreakerDamageRequest Outer;
    Outer.BaseDamage=.05f;Outer.bCanCritical=false;Outer.SetInstigator(Target);Outer.RandomSeed=LastPassiveBlockSeed;
    Outer.SourceLocation=Target->GetActorLocation();Outer.bHasSourceLocation=true;
    const auto OuterResult=Combat->ReceiveDamage(Outer);
    ASC->GetGameplayAttributeValueChangeDelegate(UBreakerAttributeSet::GetHealthAttribute()).Remove(HealthCallback);
    TestTrue(TEXT("Outer hit really resolves earned passive block"),OuterResult.bBlocked);
    TestFalse(TEXT("Outer result itself is nonlethal"),OuterResult.bKilled);
    TestTrue(TEXT("Native health callback issues lethal nested hit"),bEnteredNestedDamage&&bNestedKilled);
    TestTrue(TEXT("Nested hit leaves actual player dead"),Combat->IsDead());
    Combat->RestoreVitals();
    TestFalse(TEXT("Dead outer-hit continuation cannot arm Reprisal for revival"),Combat->HasReprisalCharge());
    return true;
}
#endif