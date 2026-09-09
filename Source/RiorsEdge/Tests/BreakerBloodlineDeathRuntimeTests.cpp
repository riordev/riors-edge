#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerTankAbilities.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerGritComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Weapons/BreakerWeaponComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerBloodlineDeathRuntimeTest,"RiorsEdge.Abilities.Tank.BloodlineDeathRuntime",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBreakerBloodlineDeathRuntimeTest::RunTest(const FString&)
{
 UWorld::InitializationValues Init;Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);if(!World)return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 const uint64 Frame=GFrameCounter;
 ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GFrameCounter=Frame;};
 World->InitializeActorsForPlay(FURL());
 auto* Player=World->SpawnActor<ABreakerCharacter>();if(!Player)return false;
 Player->bRefuseSavesForPendingCharacter=true;Player->SetActorTickEnabled(false);Player->GetBreakerMovement()->SetComponentTickEnabled(false);
 auto* ASC=Player->GetAbilitySystemComponent();auto* Attr=Player->GetAttributes();
 ASC->InitAbilityActorInfo(Player,Player);ASC->AddAttributeSetSubobject(Attr);
 auto* Combat=Player->GetCombat();Combat->BindAttributes(Attr);
 auto* P=Player->GetProgression();P->BindAttributes(Attr);
 if(!TestTrue(TEXT("Actual Tank class"),P->ChoosePermanentClassById(EBreakerClassId::Tank)))return false;
 P->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(20,P->ExperienceCurve));
 FText Reason;
 if(!P->IsAbilityUnlocked(TEXT("Tank.Bloodline")))
 {
  const int32 Before=P->GetUnspentAbilityTokens();
  if(!TestTrue(TEXT("Earned token unlocks Bloodline"),P->SpendAbilityToken(TEXT("Tank.Bloodline"),Reason)))return false;
  TestEqual(TEXT("Unlock debits actual token"),P->GetUnspentAbilityTokens(),Before-1);
 }
 const auto Slot=EBreakerAbilitySlot::ClassAbilityOne;auto* Abilities=Player->GetAbilities();
 if(!TestTrue(TEXT("Ordinary Bloodline equip"),Abilities->TryEquipAbility(Slot,TEXT("Tank.Bloodline"),Reason)))return false;
 Abilities->RefreshGrants();
 auto* Equipment=Player->GetEquipment();Equipment->BindAttributes(Attr);Equipment->BindCombatEvents();Equipment->SetComponentTickEnabled(false);Equipment->EnsureStarterKit();
 bool Found=false;
 for(int32 Seed=1;Seed<=8192;++Seed)
 {
  const auto Item=UBreakerLootLibrary::RollItem(TEXT("Bloodline.Sustain"),EBreakerEquipSlot::Gloves,EBreakerItemRarity::Exceptional,1,Seed);
  if(!Item.Affixes.ContainsByPredicate([](const FBreakerRolledAffix& A){return A.AffixId==TEXT("Core.LifeOnKill");}))continue;
  Found=Equipment->EquipItem(Item);break;
 }
 if(!TestTrue(TEXT("Unmodified real sustain drop equips"),Found)||!TestTrue(TEXT("Actual gear enables Bloodline payout"),Equipment->GetStats().LifeOnKill>0))return false;
 auto* Weapon=Player->GetWeapon();Weapon->ResetAmmunition();Weapon->SetComponentTickEnabled(false);
 auto* Enemy=World->SpawnActor<ABreakerEnemy>(FVector(300,0,100),FRotator::ZeroRotator);if(!Enemy)return false;
 Enemy->SetAreaLevel(100);Enemy->ConfigureCrowdProbe();Enemy->DispatchBeginPlay();Enemy->SetActorTickEnabled(false);
 if(auto* Move=Enemy->GetMovementComponent())Move->SetComponentTickEnabled(false);
 auto* Victim=Enemy->FindComponentByClass<UBreakerCombatComponent>();if(!Victim)return false;
 const auto* VictimAttributes=Enemy->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>();if(!VictimAttributes)return false;
 auto* Grit=Player->GetGrit();Grit->BindAttributes(Attr);Grit->SetComponentTickEnabled(false);
 auto Hurt=[&](float Amount)
 {
  FBreakerDamageRequest Hit;Hit.BaseDamage=Amount;Hit.DamageFamily=EBreakerDamageFamily::TrueDamage;Hit.bCanCritical=false;Hit.bCanBeAvoided=false;Hit.bBypassShield=true;Hit.SetInstigator(Enemy);
  return Combat->ReceiveDamage(Hit);
 };
 const float Quote=Abilities->GetResourceCostForSlot(Slot);
 // Tiny native contacts keep combat active; the real Character proximity scan
 // and normal Grit loop earn the bank without healing or resource grants.
 for(int32 I=0;I<256&&Grit->GetGrit()<Quote;++I)
 {
  Hurt(.01f);++GFrameCounter;World->Tick(LEVELTICK_All,1.f);Player->Tick(1.f);Grit->AdvanceLoop(1.f);
 }
 if(!TestTrue(TEXT("Actual nearby-enemy combat earns ordinary Grit price"),Grit->GetGrit()>=Quote))return false;
 Enemy->SetActorLocation(FVector(700,0,100)); // Move the native target beyond proximity for rifle observations.
 const float BeforeGrit=Grit->GetGrit();
 if(!TestTrue(TEXT("Actual equipped paid Bloodline activates"),Abilities->TryActivateSlot(Slot)))return false;
 TestEqual(TEXT("Bloodline pays actual quoted Grit"),BeforeGrit-Grit->GetGrit(),Quote,.001f);
 auto* State=UBreakerAbilityStateComponent::FindOrAdd(Player);State->SetComponentTickEnabled(false);
 auto* Spec=ASC->FindAbilitySpecFromClass(UBreakerAbility_Bloodline::StaticClass());
 auto* Ability=Spec?Cast<UBreakerAbility_Bloodline>(Spec->GetPrimaryInstance()):nullptr;if(!TestNotNull(TEXT("Actual native Bloodline"),Ability))return false;
 auto Clock=[&](int32 Frames){for(int32 I=0;I<Frames;++I){++GFrameCounter;World->Tick(LEVELTICK_All,.05f);State->TickComponent(.05f,LEVELTICK_All,nullptr);}};
 auto Fire=[&]()
 {
  const float Before=VictimAttributes->GetHealth();const int32 Ammo=Weapon->GetMagazineAmmo();
  Weapon->StartFire();Weapon->StopFire();
  return TestEqual(TEXT("Ordinary rifle consumes one round"),Weapon->GetMagazineAmmo(),Ammo-1)
    && TestTrue(TEXT("Actual rifle hit damages living target"),VictimAttributes->GetHealth()<Before&&!Victim->IsDead());
 };
 Hurt(40);const float BeforeLiving=Attr->GetHealth();
 if(!Fire())return false;
 TestTrue(TEXT("Living paid Bloodline retains actual sustain payout"),Attr->GetHealth()>BeforeLiving);
 const auto Death=Hurt(Combat->GetMaxHealth()*2);
 if(!TestTrue(TEXT("Native lethal hit kills owner"),Death.bKilled))return false;
 TestFalse(TEXT("Death immediately ends actual ability"),Ability->IsActive());
 TestFalse(TEXT("Death immediately closes owned window"),State->IsWindowActive(UBreakerAbility_Bloodline::WindowKey()));
 Combat->RestoreVitals();Hurt(40);Clock(6);const float BeforeRevived=Attr->GetHealth();
 if(!Fire())return false;
 TestEqual(TEXT("Restored owner receives no payout from former Bloodline"),Attr->GetHealth(),BeforeRevived,.001f);
 TestFalse(TEXT("Post-revive hit cannot rearm old window"),State->IsWindowActive(UBreakerAbility_Bloodline::WindowKey()));
 Clock(180);
 TestFalse(TEXT("Old timer cannot resurrect Bloodline"),Ability->IsActive());
 return true;
}
#endif
