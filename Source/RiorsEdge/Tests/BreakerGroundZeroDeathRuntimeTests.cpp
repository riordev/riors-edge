#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerGroundZeroDeathRuntimeTest,"RiorsEdge.Abilities.Tank.GroundZeroDeathRuntime",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBreakerGroundZeroDeathRuntimeTest::RunTest(const FString&)
{
 for (bool bDie : {false,true})
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
 if(!P->IsAbilityUnlocked(TEXT("Tank.GroundZero")))
 {
  const int32 Before=P->GetUnspentAbilityTokens();
  if(!TestTrue(TEXT("Earned token unlocks GroundZero"),P->SpendAbilityToken(TEXT("Tank.GroundZero"),Reason)))return false;
  TestEqual(TEXT("Unlock debits actual token"),P->GetUnspentAbilityTokens(),Before-1);
 }
 const auto Slot=EBreakerAbilitySlot::ClassAbilityOne;auto* Abilities=Player->GetAbilities();
 if(!TestTrue(TEXT("Ordinary GroundZero equip"),Abilities->TryEquipAbility(Slot,TEXT("Tank.GroundZero"),Reason)))return false;
 Abilities->RefreshGrants();
 auto* Equipment=Player->GetEquipment();Equipment->BindAttributes(Attr);Equipment->BindCombatEvents();Equipment->SetComponentTickEnabled(false);Equipment->EnsureStarterKit();
 auto* Enemy=World->SpawnActor<ABreakerEnemy>(FVector(300,0,100),FRotator::ZeroRotator);if(!Enemy)return false;
 Enemy->SetAreaLevel(100);Enemy->ConfigureCrowdProbe();Enemy->DispatchBeginPlay();Enemy->SetActorTickEnabled(false);
 if(auto* Move=Enemy->GetMovementComponent())Move->SetComponentTickEnabled(false);
 const auto* TargetHealth=Enemy->GetAbilitySystemComponent()->GetSet<UBreakerAttributeSet>();if(!TargetHealth)return false;
 auto* Grit=Player->GetGrit();Grit->BindAttributes(Attr);Grit->SetComponentTickEnabled(false);
 auto Hurt=[&](float Amount)
 {
  FBreakerDamageRequest Hit;Hit.BaseDamage=Amount;Hit.DamageFamily=EBreakerDamageFamily::TrueDamage;Hit.bCanCritical=false;Hit.bCanBeAvoided=false;Hit.bBypassShield=true;Hit.SetInstigator(Enemy);
  return Combat->ReceiveDamage(Hit);
 };
 const float Quote=Abilities->GetResourceCostForSlot(Slot);
 // Existing real combat/proximity loop funds the actual cast. No resource fill,
 // healing grant, cooldown removal or health-capacity editing.
 for(int32 I=0;I<256&&Grit->GetGrit()<Quote;++I)
 {
  Hurt(.01f);++GFrameCounter;World->Tick(LEVELTICK_All,1.f);Player->Tick(1.f);Grit->AdvanceLoop(1.f);
 }
 if(!TestTrue(TEXT("Actual nearby combat earns Grit price"),Grit->GetGrit()>=Quote))return false;
 auto* Floor=World->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Floor);
 Floor->AddInstanceComponent(Box);Floor->SetRootComponent(Box);Box->SetBoxExtent(FVector(2000,2000,10));
 Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionObjectType(ECC_WorldStatic);
 Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Floor->SetActorLocation(FVector(0,0,-10));
 auto* Movement=Player->GetBreakerMovement();Movement->bRunPhysicsWithNoController=true;
 Player->SetActorLocation(FVector(0,0,400));Movement->SetMovementMode(MOVE_Falling);Movement->Velocity=FVector(0,0,-100);
 const float BeforeGrit=Grit->GetGrit(),BeforeHealth=TargetHealth->GetHealth();
 if(!TestTrue(TEXT("Actual equipped airborne Ground Zero casts"),Abilities->TryActivateSlot(Slot)))return false;
 TestEqual(TEXT("Cast pays quoted Grit"),BeforeGrit-Grit->GetGrit(),Quote,.001f);
 auto* Spec=ASC->FindAbilitySpecFromClass(UBreakerAbility_GroundZero::StaticClass());
 auto* Ability=Spec?Cast<UBreakerAbility_GroundZero>(Spec->GetPrimaryInstance()):nullptr;
 if(!TestNotNull(TEXT("Real native active plunge"),Ability))return false;
 TestTrue(TEXT("Paid plunge awaits real landing"),Ability->IsActive());
 TestEqual(TEXT("Airborne activation does not fabricate blast"),TargetHealth->GetHealth(),BeforeHealth);
 if(bDie)
 {
  const auto Death=Hurt(Combat->GetMaxHealth()*2);
  if(!TestTrue(TEXT("Actual owner lethal damage"),Death.bKilled))return false;
  TestFalse(TEXT("Death cancels active plunge before any landing"),Ability->IsActive());
  Combat->RestoreVitals();
 }
 // Advance genuine CharacterMovement physics once per native world step.
 // Both living and revived pawns descend onto the same real floor.
 for(int32 I=0;I<100&&Movement->IsFalling();++I)
 {
  ++GFrameCounter;World->Tick(LEVELTICK_All,.05f);Movement->TickComponent(.05f,LEVELTICK_All,nullptr);
 }
 if(!TestTrue(TEXT("Actual native movement reaches physical floor"),Movement->IsMovingOnGround()))return false;
 TestFalse(TEXT("Landing leaves no active plunge"),Ability->IsActive());
 if(bDie)TestEqual(TEXT("Revived landing cannot detonate previous paid cast"),TargetHealth->GetHealth(),BeforeHealth,.001f);
 else TestTrue(TEXT("Ordinary living landing still pays actual enemy damage"),TargetHealth->GetHealth()<BeforeHealth);
 }
 return true;
}
#endif