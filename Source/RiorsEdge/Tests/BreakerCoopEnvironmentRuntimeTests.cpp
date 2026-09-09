#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Game/BreakerCoopCombatTest.h"
#include "Game/BreakerGameMode.h"
#include "Game/BreakerZoneBuilder.h"
#include "Combat/BreakerEnemy.h"
#include "Interaction/BreakerNPC.h"
#include "Interaction/BreakerFeedstockPickup.h"
#include "Items/BreakerLootPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "UObject/Package.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoopEnvironmentRuntimeTest,"RiorsEdge.Network.CoopLocalEnvironment",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBreakerCoopEnvironmentRuntimeTest::RunTest(const FString&)
{
 UWorld::InitializationValues Init;Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
 auto* Package=CreatePackage(*FString::Printf(TEXT("/Temp/CoopEnvironment_%s/Lvl_Fernhall"),*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
 Package->SetFlags(RF_Transient);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("Lvl_Fernhall"),Package,true,ERHIFeatureLevel::Num,&Init);
 if(!TestNotNull(TEXT("Environment test world"),World))return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL());
 ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
 TestFalse(TEXT("Ordinary world refuses opt-in local construction"),BreakerCoopCombatTest::EnsureLocalEnvironment(World));
 World->URL.AddOption(TEXT("BreakerCoopCombat=1"));
 // Invoke the exact client-side environment function without starting a
 // GameMode encounter. Network transport is covered by the two-process smoke.
 if(!TestTrue(TEXT("Native Fernhall environment builds"),BreakerCoopCombatTest::EnsureLocalEnvironment(World)))return false;
 auto CountActors=[&](){int32 N=0;for(TActorIterator<AActor> It(World);It;++It)++N;return N;};
 const int32 Before=CountActors();
 TestTrue(TEXT("Subsequent client pawn initialization succeeds"),BreakerCoopCombatTest::EnsureLocalEnvironment(World));
 TestEqual(TEXT("A second initialization creates no duplicate actors"),CountActors(),Before);
 int32 Meshes=0,Lights=0,Gameplay=0;
 for(TActorIterator<AStaticMeshActor> It(World);It;++It)if(It->GetStaticMeshComponent()->GetStaticMesh())++Meshes;
 for(TActorIterator<ADirectionalLight> It(World);It;++It)++Lights;
 for(TActorIterator<AActor> It(World);It;++It)
  if(It->IsA<ABreakerEnemy>()||It->IsA<ABreakerNPC>()||It->IsA<ABreakerLootPickup>()||It->IsA<ABreakerFeedstockPickup>())++Gameplay;
 TestTrue(TEXT("Client has authored geometry"),Meshes>0);
 TestEqual(TEXT("Client has one baseline sun"),Lights,1);
 TestEqual(TEXT("No client enemies, interactions or rewards were spawned"),Gameplay,0);
 TArray<FBreakerZonePiece> Pieces;FBreakerZoneMarkers Markers;
 if(!TestTrue(TEXT("Read actual arrival marker"),UBreakerZoneBuilder::CollectZonePieces(UBreakerZoneBuilder::FernhallMeshFolder(),Pieces)
  &&UBreakerZoneBuilder::ExtractMarkers(Pieces,Markers)))return false;
 const auto* Start=Markers.Find(EBreakerZoneMarkerRole::PlayerStart);if(!TestNotNull(TEXT("Arrival marker"),Start))return false;
 FHitResult Floor;
 TestTrue(TEXT("Arrival has real client-side blocking floor"),World->LineTraceSingleByChannel(Floor,Start->Location+FVector(0,0,200),Start->Location-FVector(0,0,200),ECC_WorldStatic));
 // Exact opt-in server spawn selector, without running the encounter builder.
 auto* Mode=World->SpawnActor<ABreakerGameMode>();if(!TestNotNull(TEXT("Spawn selector"),Mode))return false;
 AActor* First=Mode->ChoosePlayerStart_Implementation(nullptr);
 AActor* Second=Mode->ChoosePlayerStart_Implementation(nullptr);
 TestNotNull(TEXT("Host authored arrival"),First);
 TestTrue(TEXT("Late guest reuses authored arrival"),First&&Second==First);
 if(First)TestTrue(TEXT("Arrival matches native marker and capsule offset"),First->GetActorLocation().Equals(Start->Location+FVector(0,0,112)));
 return true;
}
#endif
