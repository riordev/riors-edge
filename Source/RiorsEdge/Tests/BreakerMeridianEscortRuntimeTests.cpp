#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerLocalMapComponent.h"
#include "Game/BreakerPrototypeDestinations.h"
#include "Interaction/BreakerMeridianGroundCrew.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Save/BreakerQuestContent.h"
#include "Save/BreakerSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Package.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerMeridianEscortRuntimeTest,"RiorsEdge.Campaign.MeridianGroundCrewRuntime",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBreakerMeridianEscortRuntimeTest::RunTest(const FString&)
{
 const uint64 OriginalFrame=GFrameCounter;ON_SCOPE_EXIT{GFrameCounter=OriginalFrame;};
 const auto* Definition=BreakerPrototypeDestinations::Find(TEXT("PortMeridian"));if(!Definition)return false;
 UWorld::InitializationValues Init;Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
 auto* Package=CreatePackage(*FString::Printf(TEXT("/Temp/MeridianEscort_%s/%s"),*FGuid::NewGuid().ToString(EGuidFormats::Digits),*Definition->MapName));Package->SetFlags(RF_Transient);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,FName(*Definition->MapName),Package,true,ERHIFeatureLevel::Num,&Init);if(!World)return false;
 auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(World);
 ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
 auto* Session=NewObject<UBreakerGameInstance>();World->SetGameInstance(Session);Context.OwningGameInstance=Session;
 World->InitializeActorsForPlay(FURL());
 const auto Layout=BreakerPrototypeDestinations::Build(World,Definition->Id);
 if(!TestTrue(TEXT("Actual Port Meridian layout builds completely"),Layout.bComplete))return false;
 TestEqual(TEXT("Original fixed regional defenders remain"),Layout.Enemies.Num(),18);TestEqual(TEXT("Original gates remain"),Layout.Gates.Num(),2);
 // Structural escort fixture, not combat balance: normal authored guards retain
 // their class, area level and health; AI is disabled while route/time is tested.
 for(auto* Enemy:Layout.Enemies)
 {
  Enemy->DispatchBeginPlay();Enemy->SetActorTickEnabled(false);
  if(auto* Movement=Enemy->GetMovementComponent())Movement->SetComponentTickEnabled(false);
 }
 ABreakerMeridianGroundCrew* Crew=nullptr;int32 Count=0;
 for(TActorIterator<ABreakerMeridianGroundCrew> It(World);It;++It){Crew=*It;++Count;}
 if(!TestEqual(TEXT("One actual ground crew actor spawns"),Count,1)||!Crew)return false;
 Crew->DispatchBeginPlay();
 const FVector Shelter=Crew->GetActorLocation();const FVector Extraction=Definition->Districts.Last()+Crew->ExtractionOffset;
 auto* Body=Crew->FindComponentByClass<UCapsuleComponent>();if(!Body)return false;
 FCollisionQueryParams RouteQuery(SCENE_QUERY_STAT(MeridianEscortRoute),false,Crew);
 TArray<FVector> RoutePoints;
 for(int32 Pocket=0;Pocket<Definition->Districts.Num();++Pocket)
 {
  RoutePoints.Add(Definition->Districts[Pocket]+FVector(0,0,Crew->RouteCenterHeight));
  if(Pocket+1<Definition->Districts.Num())RoutePoints.Add((Definition->Districts[Pocket]+Definition->Districts[Pocket+1])*.5f+FVector(0,0,Crew->RouteCenterHeight));
 }
 RoutePoints.Add(Extraction);FVector Previous=Shelter;
 for(FVector Point:RoutePoints)
 {
  FHitResult Hit;
  TestFalse(TEXT("Actual crew capsule sweeps each authored static route leg"),World->SweepSingleByObjectType(Hit,Previous,Point,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(Body->GetScaledCapsuleRadius(),Body->GetScaledCapsuleHalfHeight()),RouteQuery));
  TestTrue(TEXT("Each route checkpoint has an actual floor"),World->LineTraceSingleByObjectType(Hit,Point,Point-FVector(0,0,150),FCollisionObjectQueryParams(ECC_WorldStatic),RouteQuery));
  Previous=Point;
 }
 auto SpawnPlayer=[&]()
 {
  FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* P=World->SpawnActor<ABreakerCharacter>(Layout.Arrival,FRotator::ZeroRotator,Params);if(!P)return P;
  P->bRefuseSavesForPendingCharacter=true;P->SetActorTickEnabled(false);
  auto* ASC=P->GetAbilitySystemComponent();ASC->InitAbilityActorInfo(P,P);ASC->AddAttributeSetSubobject(P->GetAttributes());P->GetCombat()->BindAttributes(P->GetAttributes());
  P->GetCharacterMovement()->DisableMovement();P->GetCharacterMovement()->SetComponentTickEnabled(false);return P;
 };
 auto* Player=SpawnPlayer();if(!Player)return false;
 auto* Journal=Player->GetQuestJournal();auto* Map=Player->FindComponentByClass<UBreakerLocalMapComponent>();if(!Journal||!Map)return false;
 TestTrue(TEXT("Regional completion flag registered"),UBreakerQuestLibrary::GetRegisteredFlags().Contains(ABreakerMeridianGroundCrew::CompletionFlag()));
 TestEqual(TEXT("Crew loads its own authored dialogue"),Crew->DialogueId,FName(TEXT("MeridianGroundCrew")));
 FBreakerDialogueNode Start;TArray<FBreakerDialogueChoice> Choices;
 TestTrue(TEXT("Actual start dialogue resolves"),Crew->FindDialogueNode(Crew->ResolveStartNodeId(Journal->GetState()),Start));
 Crew->GetVisibleChoices(Start,Journal->GetState(),Choices);
 TestTrue(TEXT("Reachable dialogue exposes existing physical escort action without a fabricated receipt"),Choices.ContainsByPredicate([](const auto& C){return C.Action==EBreakerDialogueAction::StartSurvivorEscort&&C.SetsQuestFlag.IsNone();}));
 TArray<TArray<ABreakerEnemy*>> Incomplete;
 for(int32 Pocket=0;Pocket<3;++Pocket)
 {
  TArray<ABreakerEnemy*> Members;for(int32 Member=0;Member<6;++Member)Members.Add(Layout.Enemies[Pocket*6+Member]);
  Incomplete.Add(Members);
 }
 Incomplete.Last().Pop();
 auto* Partial=World->SpawnActor<ABreakerMeridianGroundCrew>(Shelter+FVector(0,-1000,0),FRotator::ZeroRotator);if(!Partial)return false;
 TestFalse(TEXT("A partial actual spawn roster cannot configure an escort"),Partial->ConfigureMeridian(Definition->Districts,Incomplete));
 Player->SetActorLocation(Partial->GetActorLocation()+FVector(0,150,0));
 TestFalse(TEXT("Partial roster cannot admit a nearby escort"),Partial->TryBeginEscort(Player));Partial->Destroy();Player->SetActorLocation(Layout.Arrival);
 TestFalse(TEXT("Remote pointer cannot start escort"),Crew->TryBeginEscort(Player));
 Player->SetActorLocation(Shelter+FVector(0,150,0));
 auto* Wall=World->SpawnActor<AActor>();auto* WallBody=NewObject<UBoxComponent>(Wall);Wall->AddInstanceComponent(WallBody);Wall->SetRootComponent(WallBody);
 WallBody->SetBoxExtent(FVector(20,500,200));WallBody->SetCollisionObjectType(ECC_WorldStatic);WallBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);WallBody->SetCollisionResponseToAllChannels(ECR_Block);WallBody->RegisterComponent();
 Wall->SetActorLocation(Shelter+FVector(0,75,0));TestFalse(TEXT("Static obstruction blocks admission"),Crew->TryBeginEscort(Player));
 Wall->SetActorLocation(Shelter+FVector(300,0,0));
 Player->SetRole(ROLE_AutonomousProxy);TestFalse(TEXT("Non-authority player cannot start escort"),Crew->TryBeginEscort(Player));Player->SetRole(ROLE_Authority);
 TestTrue(TEXT("Ordinary NPC search reaches ground crew"),Player->FindNearbyNPC()==Crew);
 if(!TestTrue(TEXT("Ordinary nearby escort begins without Erased Earth quest flags"),Crew->TryBeginEscort(Player)))return false;
 TestFalse(TEXT("Repeated admission cannot restart the clock"),Crew->TryBeginEscort(Player));
 auto Step=[&](bool Follow)
 {
  if(Follow)Player->SetActorLocation(Crew->GetActorLocation()+FVector(0,150,0));
  ++GFrameCounter;World->Tick(LEVELTICK_All,.1f);
 };
 for(int32 I=0;I<30;++I)Step(true);
 const FVector Blocked=Crew->GetActorLocation();
 TestTrue(TEXT("Real swept movement reaches but does not cross blocking wall"),Blocked.X>Shelter.X&&Blocked.X<Shelter.X+300);
 for(int32 I=0;I<10;++I)Step(true);
 TestTrue(TEXT("Blocked crew does not teleport past wall"),Crew->GetActorLocation().Equals(Blocked,1.f));Wall->Destroy();
 for(int32 I=0;I<10;++I)Step(true);
 const FVector BeforeSeparation=Crew->GetActorLocation();const float TimeBefore=Crew->GetLucidityRemaining();
 Player->SetActorLocation(Crew->GetActorLocation()+FVector(0,Crew->FollowRange+300,0));
 for(int32 I=0;I<10;++I)Step(false);
 TestTrue(TEXT("Separation physically stops the crew"),Crew->GetActorLocation().Equals(BeforeSeparation,1.f));
 TestTrue(TEXT("Attempt time still passes while separated"),Crew->GetLucidityRemaining()<TimeBefore);
 FBreakerDamageRequest Death;Death.BaseDamage=Player->GetCombat()->GetMaxHealth()*2;Death.DamageFamily=EBreakerDamageFamily::TrueDamage;Death.bCanCritical=false;Death.bCanBeAvoided=false;Death.bBypassShield=true;
 Player->GetCombat()->ReceiveDamage(Death);Step(false);
 TestEqual(TEXT("Actual player death fails the escort"),Crew->GetEscortState(),EBreakerSurvivorEscortState::Failed);
 TestTrue(TEXT("Failed attempt physically resets to shelter"),Crew->GetActorLocation().Equals(Shelter));
 TestFalse(TEXT("Failure cannot fabricate completion"),Crew->IsCompleteFor(Player));
 Player->GetCombat()->RestoreVitals();Player->SetActorLocation(Shelter+FVector(0,150,0));
 if(!TestTrue(TEXT("Revival and return permit a fresh attempt"),Crew->TryBeginEscort(Player)))return false;
 for(int32 I=0;I<120;++I)Step(true);
 TestEqual(TEXT("Uncleared first pocket holds the actual route checkpoint"),Crew->GetRouteIndex(),0);
 const FVector Waiting=Crew->GetActorLocation();
 for(int32 I=0;I<10;++I)Step(true);
 TestTrue(TEXT("Uncleared checkpoint remains physically blocked"),Crew->GetActorLocation().Equals(Waiting,1.f));
 TestFalse(TEXT("An uncleared route cannot be completed"),Crew->AreAllPocketsCleared());
 // Accepted lethal requests are explicit structural guard-death fixtures. No
 // claim about weapon throughput/encounter difficulty follows from these hits.
 for(auto* Guard:Layout.Enemies)
 {
  auto* Combat=Guard->FindComponentByClass<UBreakerCombatComponent>();if(!Combat)return false;
  FBreakerDamageRequest Kill;Kill.BaseDamage=Combat->GetMaxHealth()*2;Kill.DamageFamily=EBreakerDamageFamily::TrueDamage;Kill.bCanCritical=false;Kill.bCanBeAvoided=false;Kill.bBypassShield=true;Kill.SetInstigator(Player);
  const auto Result=Combat->ReceiveDamage(Kill);TestTrue(TEXT("Each original guard dies through actual combat authority"),Result.bKilled);
 }
 TestTrue(TEXT("All three checkpoints observe real deaths"),Crew->AreAllPocketsCleared());
 for(int32 I=0;I<1000&&Crew->IsEscortActive();++I)Step(true);
 if(!TestTrue(TEXT("Physical route reaches actual hangar extraction"),Crew->IsAtExtraction()))return false;
 TestTrue(TEXT("Escort player also arrives physically"),FVector::Dist(Player->GetActorLocation(),Extraction)<=Crew->ExtractionRadius);
 TestTrue(TEXT("Arrival commits dedicated completion"),Crew->IsCompleteFor(Player));
 TestFalse(TEXT("Regional escort never writes Erased Earth proof"),Journal->HasFlag(TEXT("Quest.Survivor.Extracted")));
 TestFalse(TEXT("Completed route cannot start again"),Crew->TryBeginEscort(Player));
 TestTrue(TEXT("Completion names actual return path"),Map->GetCampaignObjective().ToString().Contains(TEXT("Return to Anchor 13")));
 TestFalse(TEXT("Completed moving objective marker disappears"),Map->GetMarkers().ContainsByPredicate([](const auto& M){return M.Id==FName(TEXT("PortMeridian.GroundCrew"));}));
 auto* Written=NewObject<UBreakerSaveGame>();Written->QuestFlags=Journal->GetState().Flags;Written->QuestCounters=Journal->GetState().Counters;
 TArray<uint8> Bytes;if(!TestTrue(TEXT("Completion serializes through save archive"),UGameplayStatics::SaveGameToMemory(Written,Bytes)))return false;
 auto* Read=Cast<UBreakerSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));auto* Returning=SpawnPlayer();if(!Read||!Returning)return false;
 Returning->GetQuestJournal()->RestoreFrom(Read->QuestFlags,Read->QuestCounters);
 auto* Revisited=World->SpawnActor<ABreakerMeridianGroundCrew>(Shelter,FRotator::ZeroRotator);if(!Revisited)return false;Revisited->DispatchBeginPlay();
 TestTrue(TEXT("Fresh regional actor reads restored completion"),Revisited->IsCompleteFor(Returning));
 TestEqual(TEXT("Revisit resolves authored safe dialogue"),Revisited->ResolveStartNodeId(Returning->GetQuestJournal()->GetState()),FName(TEXT("Safe")));
 TestFalse(TEXT("Revisit cannot restart completed quest"),Revisited->TryBeginEscort(Returning));
 return true;
}
#endif
