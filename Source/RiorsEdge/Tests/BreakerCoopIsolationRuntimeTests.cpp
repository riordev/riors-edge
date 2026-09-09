#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Game/BreakerCoopCombatTest.h"
#include "Game/BreakerGameInstance.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Save/BreakerCharacterRoster.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoopIsolationRuntimeTest,"RiorsEdge.Network.CoopCombatIsolation",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBreakerCoopIsolationRuntimeTest::RunTest(const FString&)
{
 UWorld::InitializationValues Init;Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);if(!World)return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
 TestFalse(TEXT("Ordinary world remains outside test mode"),BreakerCoopCombatTest::IsEnabled(World));
 World->URL.AddOption(TEXT("BreakerCoopCombat=1"));
 auto* Session=NewObject<UBreakerGameInstance>(GEngine);World->SetGameInstance(Session);
 const FGuid HostSavedId=FGuid::NewGuid();Session->ActiveCharacterId=HostSavedId;
 const FString SavedSlot=UBreakerCharacterRoster::SlotNameForCharacter(HostSavedId);
 TestFalse(TEXT("Fresh unrelated saved identity has no disk file"),UGameplayStatics::DoesSaveGameExist(SavedSlot,0));
 auto Make=[&](){FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* P=World->SpawnActor<ABreakerCharacter>(FVector::ZeroVector,FRotator::ZeroRotator,Params);if(!P)return P;
 auto* ASC=P->GetAbilitySystemComponent();ASC->InitAbilityActorInfo(P,P);ASC->AddAttributeSetSubobject(P->GetAttributes());P->GetCombat()->BindAttributes(P->GetAttributes());P->GetProgression()->BindAttributes(P->GetAttributes());
 P->InitializeCoopCombatProfile();return P;};
 auto* Host=Make();auto* Guest=Make();if(!Host||!Guest)return false;
 TestTrue(TEXT("Server assigns valid isolated identity"),Host->GetCoopCombatProfileId().IsValid());
 TestTrue(TEXT("Guest has different server identity"),Guest->GetCoopCombatProfileId().IsValid()&&Guest->GetCoopCombatProfileId()!=Host->GetCoopCombatProfileId());
 for(auto* P:{Host,Guest})
 {
 P->AdoptSessionCharacter();P->LoadGameState();P->EnterWorldAsCharacter(HostSavedId);P->SaveGameState();
 TestFalse(TEXT("No test pawn adopts or imports host's saved identity"),P->ActiveCharacterId.IsValid());
 TestTrue(TEXT("All callbacks remain explicitly nonsaving"),P->bRefuseSavesForPendingCharacter);
 TestFalse(TEXT("Test wallet never binds to real account"),P->bRiftglassBoundToAccount);
 TestEqual(TEXT("Server kit is fixed Swift"),P->GetProgression()->GetProgressionState().PermanentClass,EBreakerClassId::Swift);
 }
 TestEqual(TEXT("Host GameInstance identity unchanged"),Session->ActiveCharacterId,HostSavedId);
 TestFalse(TEXT("Save callbacks wrote no imported character file"),UGameplayStatics::DoesSaveGameExist(SavedSlot,0));
 FBreakerDamageRequest Hit;Hit.BaseDamage=10;Hit.bCanBeAvoided=false;Hit.bCanCritical=false;Hit.SetInstigator(Host);
 const float Before=Guest->GetAttributes()->GetHealth();Guest->GetCombat()->ReceiveDamage(Hit);
 TestEqual(TEXT("Coop teammates cannot damage one another"),Guest->GetAttributes()->GetHealth(),Before);
 Hit.SetInstigator(Guest);Guest->GetCombat()->ReceiveDamage(Hit);
 TestTrue(TEXT("Authored self damage still works"),Guest->GetAttributes()->GetHealth()<Before);
 return true;
}
#endif
