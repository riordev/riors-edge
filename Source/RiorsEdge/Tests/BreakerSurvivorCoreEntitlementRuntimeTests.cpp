#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/BreakerGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerWorldPoints.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"
#include "Save/BreakerQuestJournal.h"
#include "Save/BreakerSaveGame.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSurvivorCoreEntitlementRuntimeTest,
 "RiorsEdge.Progression.WorldPoints.SurvivorArrivalAndStartup",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBreakerSurvivorCoreEntitlementRuntimeTest::RunTest(const FString&)
{
 UWorld::InitializationValues Init;Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);if(!World)return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 // Only save I/O is suppressed by the existing opt-in mode. Native Character
 // BeginPlay, restored quest settlement and live reward callbacks still run.
 World->URL.AddOption(TEXT("BreakerCoopCombat=1"));
 auto* Session=NewObject<UBreakerGameInstance>(GEngine);World->SetGameInstance(Session);
 ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
 const FName Source(TEXT("SurvivorToAnchor"));const FName Receipt=UBreakerWorldPointLibrary::FlagForSource(Source);
 FBreakerQuestFlagSet PriorFlags;
 for(const auto& Mission:UBreakerMissionLibrary::GetMissions()) if(Mission.Act<3)
  for(const auto& Beat:Mission.Beats)for(FName Flag:UBreakerMissionLibrary::BeatCompletionFlags(Beat))PriorFlags.Add(Flag);
 int32 SpawnIndex=0;
 auto MakePlayer=[&](const UBreakerSaveGame* Saved)->ABreakerCharacter*
 {
  FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* P=World->SpawnActor<ABreakerCharacter>(FVector(++SpawnIndex*1000,0,100),FRotator::ZeroRotator,Params);
  if(!P)return nullptr;
  auto* ASC=P->GetAbilitySystemComponent();ASC->InitAbilityActorInfo(P,P);ASC->AddAttributeSetSubobject(P->GetAttributes());
  P->GetCombat()->BindAttributes(P->GetAttributes());P->GetProgression()->BindAttributes(P->GetAttributes());
  P->InitializeCoopCombatProfile();
  if(Saved){P->GetProgression()->LoadProgressionState(Saved->Progression);P->GetQuestJournal()->RestoreFrom(Saved->QuestFlags,Saved->QuestCounters);}
  else P->GetQuestJournal()->RestoreFrom(PriorFlags.Flags,PriorFlags.Counters);
  return P;
 };
 auto RoundTrip=[&](UBreakerSaveGame* Saved)->UBreakerSaveGame*
 {
  TArray<uint8> Bytes;
  if(!TestTrue(TEXT("Actual save archive writes to memory only"),UGameplayStatics::SaveGameToMemory(Saved,Bytes)))return nullptr;
  return Cast<UBreakerSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
 };
 auto* Live=MakePlayer(nullptr);if(!Live)return false;Live->DispatchBeginPlay();
 auto* Progression=Live->GetProgression();auto* Journal=Live->GetQuestJournal();
 const FBreakerProgressionState BeforeArrivalState=Progression->GetProgressionState();
 const int32 Before=BeforeArrivalState.UnspentCorePoints;
 TestFalse(TEXT("No rescue point from earlier campaign completion"),Journal->HasFlag(Receipt));
 Journal->SetFlag(BreakerQuestFlags::SurvivorAccepted);
 TestTrue(TEXT("An early Hub visit cannot produce rescue arrival"),UBreakerMissionLibrary::ArrivalFlagsFor(TEXT("Hub"),Journal->GetState()).IsEmpty());
 for(FName Flag:UBreakerMissionLibrary::ArrivalFlagsFor(TEXT("Earth.Unindustrialized"),Journal->GetState()))Journal->SetFlag(Flag);
 Journal->SetFlag(BreakerQuestFlags::SurvivorMet);
 for(FName Flag:UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(TEXT("earth.survivor_extraction"),Journal->GetState()))Journal->SetFlag(Flag);
 TestFalse(TEXT("Physical extraction without Anchor does not grant point"),Journal->HasFlag(Receipt));
 TestEqual(TEXT("Wallet unchanged before actual destination proof"),Progression->GetProgressionState().UnspentCorePoints,Before);
 TestTrue(TEXT("Wrong return destination grants no arrival"),UBreakerMissionLibrary::ArrivalFlagsFor(TEXT("Fernhall"),Journal->GetState()).IsEmpty());
 int32 ReceiptEvents=0,SaveEvents=0;
 Journal->OnFlagSet.AddLambda([&](FName Flag){if(Flag==Receipt){++ReceiptEvents;
  TestEqual(TEXT("Receipt observers already see the funded wallet"),Progression->GetProgressionState().UnspentCorePoints,Before+1);
  Progression->SettleWorldCorePoints(Journal);}});
 Journal->OnPersistRequested.AddLambda([&](){if(Journal->HasFlag(Receipt)){++SaveEvents;
  TestEqual(TEXT("Synchronous live persistence sees point and receipt together"),Progression->GetProgressionState().UnspentCorePoints,Before+1);}});
 const auto Arrival=UBreakerMissionLibrary::ArrivalFlagsFor(TEXT("Hub"),Journal->GetState());
 if(!TestEqual(TEXT("Supported destination event produces one arrival flag"),Arrival.Num(),1))return false;
 for(FName Flag:Arrival)Journal->SetFlag(Flag);
 TestTrue(TEXT("Existing flag reward binding grants canonical receipt"),Journal->HasFlag(Receipt));
 TestEqual(TEXT("Arrival grants exactly one Core point"),Progression->GetProgressionState().UnspentCorePoints,Before+1);
 TestEqual(TEXT("Reentrant settlement cannot republish receipt"),ReceiptEvents,1);
 TestTrue(TEXT("Synchronous persistence was exercised"),SaveEvents>0);
 TestFalse(TEXT("Arrival does not fabricate quest turn-in"),Journal->HasFlag(BreakerQuestFlags::SurvivorTurnedIn));
 TestFalse(TEXT("Repeated arrival flag is rejected"),Journal->SetFlag(Arrival[0]));
 Progression->SettleWorldCorePoints(Journal);
 TestEqual(TEXT("Repeated arrival/settlement pays nothing extra"),Progression->GetProgressionState().UnspentCorePoints,Before+1);

 // Historical archive: same genuinely completed arrival flags, but written
 // before this source had an Unlock beat, so no new receipt/point exists.
 auto* Historical=NewObject<UBreakerSaveGame>();Historical->Progression=BeforeArrivalState;
 Historical->QuestFlags=Journal->GetFlags();Historical->QuestFlags.Remove(Receipt);Historical->QuestCounters=Journal->GetState().Counters;
 auto* Loaded=RoundTrip(Historical);if(!TestNotNull(TEXT("Historical archive reloads"),Loaded))return false;
 auto* Restored=MakePlayer(Loaded);if(!Restored)return false;
 auto* RestoredProgression=Restored->GetProgression();auto* RestoredJournal=Restored->GetQuestJournal();
 TestFalse(TEXT("Silent restore does not publish an unearned receipt"),RestoredJournal->HasFlag(Receipt));
 const int32 RestoreBefore=RestoredProgression->GetProgressionState().UnspentCorePoints;
 auto* SavedSynchronously=NewObject<UBreakerSaveGame>();int32 StartupSaves=0,StartupReceipts=0;
 RestoredJournal->OnFlagSet.AddLambda([&](FName Flag){if(Flag==Receipt){++StartupReceipts;RestoredProgression->SettleWorldCorePoints(RestoredJournal);}});
 RestoredJournal->OnPersistRequested.AddLambda([&](){if(RestoredJournal->HasFlag(Receipt)){
  ++StartupSaves;SavedSynchronously->Progression=RestoredProgression->GetProgressionState();SavedSynchronously->QuestFlags=RestoredJournal->GetFlags();SavedSynchronously->QuestCounters=RestoredJournal->GetState().Counters;
  TestEqual(TEXT("Startup save contains funded wallet before callback returns"),SavedSynchronously->Progression.UnspentCorePoints,RestoreBefore+1);}});
 // Actual native startup; no SetFlag or direct Settle call initiates this grant.
 Restored->DispatchBeginPlay();
 TestTrue(TEXT("Startup backfills completed historical arrival"),RestoredJournal->HasFlag(Receipt));
 TestEqual(TEXT("Startup adds exactly one point"),RestoredProgression->GetProgressionState().UnspentCorePoints,RestoreBefore+1);
 TestEqual(TEXT("Startup receipt publishes once under reentry"),StartupReceipts,1);
 if(!TestTrue(TEXT("Startup performed synchronous save notification"),StartupSaves>0))return false;
 auto* Claimed=RoundTrip(SavedSynchronously);if(!Claimed)return false;
 auto* Reopened=MakePlayer(Claimed);if(!Reopened)return false;
 const int32 ReopenBefore=Reopened->GetProgression()->GetProgressionState().UnspentCorePoints;
 Reopened->DispatchBeginPlay();
 TestTrue(TEXT("Saved receipt survives actual archive reload"),Reopened->GetQuestJournal()->HasFlag(Receipt));
 TestEqual(TEXT("Second startup cannot duplicate claimed reward"),Reopened->GetProgression()->GetProgressionState().UnspentCorePoints,ReopenBefore);
 return true;
}
#endif
