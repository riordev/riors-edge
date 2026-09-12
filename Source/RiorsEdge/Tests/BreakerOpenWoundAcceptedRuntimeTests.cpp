#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerTankAbilities.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerGritComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDeployable.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Weapons/BreakerRocketProjectile.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "Weapons/BreakerWeaponComponent.h"

#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerOpenWoundAcceptedTest,"RiorsEdge.Abilities.OpenWoundAcceptedRend",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerOpenWoundAcceptedTest::RunTest(const FString& Parameters)
{
    FBreakerItemInstance Gear;
    for (int32 Seed=1;Seed<=4096;++Seed)
    {
        Gear=UBreakerLootLibrary::RollItem(TEXT("OpenWound.Heal"),EBreakerEquipSlot::Gloves,EBreakerItemRarity::Standard,1,Seed);
        if (Gear.Affixes.ContainsByPredicate([](const auto& Line){return Line.AffixId==FName(TEXT("Core.LifeOnKill"));})) break;
    }
    if (!TestTrue(TEXT("ordinary complete rolled gloves contain existing sustain stat"),Gear.IsValid() && Gear.Affixes.ContainsByPredicate([](const auto& Line){return Line.AffixId==FName(TEXT("Core.LifeOnKill"));}))) return false;
    // O272: Open Wound is a single-rank travel that pays every accepted
    // target. Scenarios: 0 avoided, 1 immune, 2 first avoided / second
    // accepted, 3 two accepted, 4 shield-only hit, 5 lethal hit.
    for (int32 Scenario=0;Scenario<6;++Scenario)
    {
        UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
        if (!World) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
        ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Tank=World->SpawnActor<ABreakerCharacter>(FVector(0,0,200),FRotator::ZeroRotator,Spawn);
        auto* Front=World->SpawnActor<ABreakerCharacter>(FVector(110,0,200),FRotator(0,180,0),Spawn);
        auto* Rear=World->SpawnActor<ABreakerCharacter>(FVector(240,0,200),FRotator(0,180,0),Spawn);
        if (!Tank || !Front || !Rear) return false;
        for (auto* Pawn:{Tank,Front,Rear})
        {
            Pawn->SetActorTickEnabled(false); Pawn->GetBreakerMovement()->SetComponentTickEnabled(false);
            auto* ASC=Pawn->GetAbilitySystemComponent(); ASC->InitAbilityActorInfo(Pawn,Pawn); ASC->AddAttributeSetSubobject(Pawn->GetAttributes());
            Pawn->GetCombat()->BindAttributes(Pawn->GetAttributes()); Pawn->GetProgression()->BindAttributes(Pawn->GetAttributes());
            Pawn->GetCombat()->DodgeChance=0; Pawn->GetCombat()->BlockChance=0;
        }
        auto* Progression=Tank->GetProgression();
        if (!Progression->ChoosePermanentClassById(EBreakerClassId::Tank)) return false;
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50,Progression->ExperienceCurve));
        FBreakerQuestFlagSet Flags;
        for (const auto& Mission:UBreakerMissionLibrary::GetMissions())
            for (const auto& Beat:Mission.Beats)
                for (FName Flag:UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
        Flags.Add(TEXT("Quest.Finale.Seal")); Progression->SettleDoctrineEntitlement(Flags);
        FText Reason;
        if (!TestTrue(TEXT("paid Open Wound travel with one point"),Progression->PurchaseNode(UBreakerProgressionLibrary::GetTankLeechTree(),TEXT("Tank.Leech.OpenWound"),Reason))) return false;
        Tank->GetEquipment()->BindAttributes(Tank->GetAttributes());
        if (!TestTrue(TEXT("complete rolled sustain gloves actually equip"),Tank->GetEquipment()->EquipItem(Gear))) return false;
        const float LifeOnKill=Tank->GetEquipment()->GetStats().LifeOnKill;
        if (!TestTrue(TEXT("existing stand-in magnitude is positive"),LifeOnKill>0)) return false;
        auto* Grit=Tank->GetGrit(); Grit->BindAttributes(Tank->GetAttributes()); Grit->SetComponentTickEnabled(false); Grit->SetInCombat(true);
        for (int32 N=0;N<70;++N) { Grit->SetEnemyInProximity(true); Grit->AdvanceLoop(1); }
        Tank->GetWeapon()->BeginPlay(); Tank->GetWeapon()->EquipArchetype(EBreakerWeaponArchetype::Rifle);
        // Real damage creates healing headroom, not a resource or offense grant.
        FBreakerDamageRequest Hurt; Hurt.BaseDamage=Tank->GetAttributes()->GetMaxHealth()*.65f;
        Hurt.DamageFamily=EBreakerDamageFamily::TrueDamage; Hurt.bCanCritical=false; Hurt.bCanBeAvoided=false;
        Tank->GetCombat()->ReceiveDamage(Hurt);
        if (Scenario==0 || Scenario==2) Front->GetCombat()->DodgeChance=1;
        if (Scenario==1) Front->GetCombat()->PushIncomingDamageModifier(TEXT("Fixture.Immunity"),0);
        if (Scenario<2 || Scenario>=4) Rear->SetActorLocation(FVector(2000,0,200));
        if (Scenario==4) { Front->GetAttributes()->ApplyMaxShield(1000); Front->GetAttributes()->ApplyShield(1000); } // Explicit target shield fixture.
        if (Scenario==5) Front->GetAttributes()->ApplyHealth(1); // Explicit lethal target fixture.
        const float Before=Tank->GetAttributes()->GetHealth();
        const float FrontHealth=Front->GetAttributes()->GetHealth(), RearHealth=Rear->GetAttributes()->GetHealth();
        const float Shield=Front->GetAttributes()->GetShield();
        const float Resource=Tank->GetAttributes()->GetClassResource();
        auto* ASC=Tank->GetAbilitySystemComponent();
        const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(UBreakerAbility_Rend::StaticClass(),1));
        if (!TestTrue(TEXT("actual paid Rend"),ASC->TryActivateAbility(Handle))) return false;
        TestTrue(TEXT("Rend spends ordinary funded Grit"),Tank->GetAttributes()->GetClassResource()<Resource);
        const float Dealt=FrontHealth-Front->GetAttributes()->GetHealth()+RearHealth-Rear->GetAttributes()->GetHealth()+Shield-Front->GetAttributes()->GetShield();
        const int32 Accepted=Scenario<2 ? 0 : (Scenario==3 ? 2 : 1);
        const float NodeHeal=LifeOnKill*Accepted;   // rank one pays every accepted target
        // Equipment BeginPlay is deliberately absent in this isolated fixture;
        // its separate OnKillDealt sustain listener is therefore not bound.
        // The lethal row proves Rend's accepted-hit payout, not gear kill income.
        const float Expected=Dealt*GetDefault<UBreakerAbility_Rend>()->HealFraction+NodeHeal;
        TestEqual(FString::Printf(TEXT("scenario%d pays accepted hit heal only"),Scenario),Tank->GetAttributes()->GetHealth()-Before,Expected,.01f);
        if (Scenario<2) TestEqual(TEXT("refused target takes no damage"),Dealt,0.0f);
        else TestTrue(TEXT("accepted target takes actual damage"),Dealt>0);
        if (Scenario==2) TestEqual(TEXT("avoided first target stays intact; the later accepted hit pays"),Front->GetAttributes()->GetHealth(),FrontHealth);
        if (Scenario==4) TestEqual(TEXT("shield-only hit counts without health damage"),Front->GetAttributes()->GetHealth(),FrontHealth);
        if (Scenario==5) TestTrue(TEXT("lethal hit still counts"),Front->GetCombat()->IsDead());
    }
    return true;
}
#endif
