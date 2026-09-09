#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "Abilities/BreakerTankAbilities.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerGritComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerExperience.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerProvokeThreatRuntimeTest,"RiorsEdge.Abilities.Tank.ProvokeThreatRuntime",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBreakerProvokeThreatRuntimeTest::RunTest(const FString&)
{
 UWorld::InitializationValues Init;Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);if(!World)return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 const uint64 Frame=GFrameCounter;ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);GFrameCounter=Frame;};
 auto Spawn=[&](FVector At){auto* P=World->SpawnActor<ABreakerCharacter>(At,FRotator::ZeroRotator);if(!P)return P;
 P->bRefuseSavesForPendingCharacter=true;P->SetActorTickEnabled(false);P->GetBreakerMovement()->SetComponentTickEnabled(false);
 auto* ASC=P->GetAbilitySystemComponent();ASC->InitAbilityActorInfo(P,P);ASC->AddAttributeSetSubobject(P->GetAttributes());P->GetCombat()->BindAttributes(P->GetAttributes());return P;};
 auto* Tank=Spawn(FVector(0,0,100));auto* Other=Spawn(FVector(1200,300,100));if(!Tank||!Other)return false;
 auto* P=Tank->GetProgression();P->BindAttributes(Tank->GetAttributes());if(!P->ChoosePermanentClassById(EBreakerClassId::Tank))return false;
 P->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(20,P->ExperienceCurve));FText Reason;
 if(!P->PurchaseNode(UBreakerProgressionLibrary::GetCoreSliceTree(),TEXT("Core.Threat.Presence"),Reason))return false;
 if(!P->IsAbilityUnlocked(TEXT("Tank.Provoke"))&&!P->SpendAbilityToken(TEXT("Tank.Provoke"),Reason))return false;
 auto* Abilities=Tank->GetAbilities();if(!Abilities->TryEquipAbility(EBreakerAbilitySlot::ClassAbilityOne,TEXT("Tank.Provoke"),Reason))return false;Abilities->RefreshGrants();
 auto* Enemy=World->SpawnActor<ABreakerEnemy>(FVector(0,300,100),FRotator::ZeroRotator);if(!Enemy)return false;
 Enemy->ConfigureCrowdProbe();Enemy->SetAreaLevel(100);Enemy->DispatchBeginPlay();Enemy->SetActorTickEnabled(false);
 if(auto* Movement=Enemy->FindComponentByClass<UPawnMovementComponent>()){Movement->StopMovementImmediately();Movement->SetComponentTickEnabled(false);}
 auto* Sink=Enemy->FindComponentByClass<UBreakerCombatComponent>();auto* Health=FindObject<UBreakerAttributeSet>(Enemy,TEXT("Attributes"));if(!Sink||!Health)return false;
 auto* Grit=Tank->GetGrit();Grit->BindAttributes(Tank->GetAttributes());Grit->SetComponentTickEnabled(false);
 // Native incoming contacts and actual character proximity scan fund Grit.
 for(int32 I=0;I<70;++I){FBreakerDamageRequest Contact;Contact.BaseDamage=.01f;Contact.bCanCritical=false;Contact.bCanBeAvoided=false;Contact.SetInstigator(Enemy);
 Tank->GetCombat()->ReceiveDamage(Contact);Tank->Tick(.3f);Grit->AdvanceLoop(1);}
 if(!TestTrue(TEXT("Ordinary proximity funds cast"),Grit->GetGrit()>=Abilities->GetResourceCostForSlot(EBreakerAbilitySlot::ClassAbilityOne)))return false;
 auto Hit=[&](AActor* Source,float Amount){FBreakerDamageRequest H;H.BaseDamage=Amount;H.bCanCritical=false;H.bCanBeAvoided=false;H.DamageFamily=EBreakerDamageFamily::TrueDamage;H.SetInstigator(Source);return Sink->ReceiveDamage(H);};
 auto Select=[&](){Enemy->Tick(0);return Enemy->GetThreatTarget();};
 Hit(Other,10);TestTrue(TEXT("Real competitor threat initially wins"),Select()==Other);
 const float BeforeHealth=Health->GetHealth(),BeforeGrit=Grit->GetGrit();const int32 BeforeXp=P->GetProgressionState().TotalExperience;
 const float Price=Abilities->GetResourceCostForSlot(EBreakerAbilitySlot::ClassAbilityOne);
 if(!TestTrue(TEXT("Unlocked equipped Provoke casts"),Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityOne)))return false;
 TestEqual(TEXT("Cast pays quoted Grit"),BeforeGrit-Grit->GetGrit(),Price,.001f);
 TestEqual(TEXT("Threat grant inflicts no damage"),Health->GetHealth(),BeforeHealth);
 TestEqual(TEXT("Threat grant awards no experience"),P->GetProgressionState().TotalExperience,BeforeXp);
 TestTrue(TEXT("Window forces Tank target"),Select()==Tank);
 const auto* Defaults=GetDefault<UBreakerAbility_Provoke>();
 const float Grant=Defaults->ThreatGranted*P->GetNodeStats().ThreatGeneratedMultiplier;
 // Between unscaled and scaled grant: the authored Presence point matters after expiry.
 Hit(Other,Defaults->ThreatGranted+1-10);
 TestTrue(TEXT("Forced target holds despite competitor score"),Select()==Tank);
 for(float T=0;T<Defaults->ForcedTargetSeconds+.05f;T+=.01f){++GFrameCounter;World->Tick(LEVELTICK_All,.01f);}
 TestTrue(TEXT("Scaled real grant retains Tank after window closes"),Select()==Tank);
 if(!TestFalse(TEXT("Threat competitor survives outside melee reach"),Other->GetCombat()->IsDead()))return false;
 Hit(Other,Grant+1);TestTrue(TEXT("Stronger later competitor wins after forced window"),Select()==Other);
 // Restore actual completed campaign benchmarks, then settle entitlement. This
 // fixture exercises earned entitlement/purchases, not a full campaign replay.
 FBreakerQuestFlagSet Flags;
 for(const auto& Mission:UBreakerMissionLibrary::GetMissions())
   for(const auto& Beat:Mission.Beats)
     for(FName Flag:UBreakerMissionLibrary::BeatCompletionFlags(Beat))Flags.Add(Flag);
 P->SettleDoctrineEntitlement(Flags);
 TestEqual(TEXT("Actual campaign supplies eight Doctrine points"),P->GetProgressionState().UnspentDoctrinePoints,8);
 auto* Bastion=UBreakerProgressionLibrary::GetTankBastionTree();
 if(!TestTrue(TEXT("Commit Bastion"),P->CommitToBranch(Bastion->TreeId,Reason)))return false;
 for(const TCHAR* Id:{TEXT("Tank.Bastion.Loud"),TEXT("Tank.Bastion.Loud"),TEXT("Tank.Bastion.AnsweringFire"),TEXT("Tank.Bastion.AnsweringFire")})
   if(!TestTrue(TEXT("Pay actual Standing Order prerequisite"),P->PurchaseNode(Bastion,Id,Reason)))return false;
 TestFalse(TEXT("Standing Order refuses before six-point tier investment"),P->PurchaseNode(Bastion,TEXT("Tank.Bastion.StandingOrder"),Reason));
 for(const TCHAR* Id:{TEXT("Tank.Bastion.Footing"),TEXT("Tank.Bastion.Footing"),TEXT("Tank.Bastion.StandingOrder")})
   if(!TestTrue(TEXT("Pay remaining actual Doctrine route"),P->PurchaseNode(Bastion,Id,Reason)))return false;
 TestEqual(TEXT("Standing Order route spends exactly eight Doctrine points"),P->GetProgressionState().UnspentDoctrinePoints,0);
 const auto* Provoke=UBreakerAbilityDefinition::FindFallback(TEXT("Tank.Provoke"));
 if(!Provoke)return false;
 for(float T=0;T<Provoke->GetCooldownSeconds()+.05f;T+=.01f){++GFrameCounter;World->Tick(LEVELTICK_All,.01f);}
 // Replenish only through the same actual incoming/proximity sources.
 for(int32 I=0;I<70;++I){FBreakerDamageRequest Contact;Contact.BaseDamage=.01f;Contact.bCanCritical=false;Contact.bCanBeAvoided=false;Contact.SetInstigator(Enemy);
   Tank->GetCombat()->ReceiveDamage(Contact);Tank->Tick(.3f);Grit->AdvanceLoop(1);}
 const float StandingPrice=Abilities->GetResourceCostForSlot(EBreakerAbilitySlot::ClassAbilityOne);
 const float StandingBefore=Grit->GetGrit();
 if(!TestTrue(TEXT("Earned Grit funds rewritten cast"),StandingBefore>=StandingPrice))return false;
 if(!TestTrue(TEXT("Purchased Standing Order uses actual equipped Provoke"),Abilities->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityOne)))return false;
 TestEqual(TEXT("Rewritten cast pays actual quote"),StandingBefore-Grit->GetGrit(),StandingPrice,.001f);
 TestTrue(TEXT("Owned Standing Order forces Tank"),Select()==Tank);
 Hit(Tank,1);TestTrue(TEXT("Taunter's damage preserves Standing Order"),Select()==Tank);
 // A new larger competitor score must not outbid the forced rewrite before
 // its foreign-damage cancellation; this hit is exactly that cancellation.
 Hit(Other,Grant*3);TestTrue(TEXT("Foreign real damage cancels purchased Standing Order focus"),Select()==Other);
 Enemy->ApplyProvokeThreat(Tank,0,4,false);TestTrue(TEXT("Forced focus starts before range test"),Select()==Tank);
 Tank->SetActorLocation(FVector(100000,0,100));
 TestTrue(TEXT("Out-of-range taunter cannot suppress valid nearby competitor"),Select()==Other);
 return true;
}
#endif
