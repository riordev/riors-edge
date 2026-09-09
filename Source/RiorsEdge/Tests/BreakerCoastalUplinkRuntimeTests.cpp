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
#include "Interaction/BreakerCoastalUplink.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Save/BreakerQuestContent.h"
#include "Save/BreakerSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Package.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoastalUplinkRuntimeTest,"RiorsEdge.Campaign.CoastalUplinkRuntime",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBreakerCoastalUplinkRuntimeTest::RunTest(const FString&)
{
 const uint64 OriginalFrame=GFrameCounter;ON_SCOPE_EXIT{GFrameCounter=OriginalFrame;};
 const auto* Definition=BreakerPrototypeDestinations::Find(TEXT("BrokenCoast"));if(!Definition)return false;
 UWorld::InitializationValues Init;Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
 auto* Package=CreatePackage(*FString::Printf(TEXT("/Temp/Uplink_%s/%s"),*FGuid::NewGuid().ToString(EGuidFormats::Digits),*Definition->MapName));Package->SetFlags(RF_Transient);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,FName(*Definition->MapName),Package,true,ERHIFeatureLevel::Num,&Init);if(!World)return false;
 auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(World);
 ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
 auto* Session=NewObject<UBreakerGameInstance>();World->SetGameInstance(Session);Context.OwningGameInstance=Session;
 World->InitializeActorsForPlay(FURL());
 const auto Layout=BreakerPrototypeDestinations::Build(World,Definition->Id);
 if(!TestTrue(TEXT("Actual coast layout builds completely"),Layout.bComplete))return false;
 TestEqual(TEXT("Original finite regional defenders remain"),Layout.Enemies.Num(),18);
 TestEqual(TEXT("Original return gates remain"),Layout.Gates.Num(),2);
 // Isolate objective timing from AI pressure; do not change their health, damage,
 // level or death state. This is not an encounter/balance acceptance fixture.
 for(auto* Enemy:Layout.Enemies)
 {
  Enemy->SetActorTickEnabled(false);
  if(auto* Movement=Enemy->GetMovementComponent())Movement->SetComponentTickEnabled(false);
 }
 ABreakerCoastalUplink* Uplink=nullptr;int32 Count=0;
 for(TActorIterator<ABreakerCoastalUplink> It(World);It;++It){Uplink=*It;++Count;}
 if(!TestEqual(TEXT("One actual Signal Point objective spawns"),Count,1)||!Uplink)return false;
 Uplink->DispatchBeginPlay();
 auto SpawnPlayer=[&]()
 {
  FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* P=World->SpawnActor<ABreakerCharacter>(Layout.Arrival,FRotator::ZeroRotator,Params);if(!P)return P;
  P->bRefuseSavesForPendingCharacter=true;
  auto* ASC=P->GetAbilitySystemComponent();ASC->InitAbilityActorInfo(P,P);ASC->AddAttributeSetSubobject(P->GetAttributes());P->GetCombat()->BindAttributes(P->GetAttributes());
  P->GetCharacterMovement()->DisableMovement();P->GetCharacterMovement()->SetComponentTickEnabled(false);return P;
 };
 auto* Player=SpawnPlayer();if(!Player)return false;
 auto* Journal=Player->GetQuestJournal();auto* Map=Player->FindComponentByClass<UBreakerLocalMapComponent>();if(!Journal||!Map)return false;
 const FVector Approach=Uplink->GetActorLocation()+FVector(-150,0,0);
 FCollisionQueryParams Geometry(SCENE_QUERY_STAT(CoastalUplinkClearance),false,Uplink);Geometry.AddIgnoredActor(Player);
 auto* ConsoleBody=Uplink->FindComponentByClass<UCapsuleComponent>();if(!ConsoleBody)return false;
 TestFalse(TEXT("Console capsule clears actual tower and cover"),World->OverlapAnyTestByObjectType(ConsoleBody->GetComponentLocation(),ConsoleBody->GetComponentQuat(),FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(ConsoleBody->GetScaledCapsuleRadius(),ConsoleBody->GetScaledCapsuleHalfHeight()),Geometry));
 auto* PlayerBody=Player->GetCapsuleComponent();
 TestFalse(TEXT("Standing approach capsule clears actual geometry"),World->OverlapAnyTestByObjectType(Approach,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(PlayerBody->GetScaledCapsuleRadius(),PlayerBody->GetScaledCapsuleHalfHeight()),Geometry));
 TestTrue(TEXT("Dedicated completion flag registered"),UBreakerQuestLibrary::GetRegisteredFlags().Contains(ABreakerCoastalUplink::CompletionFlag()));
 TestTrue(TEXT("Live objective map marker exists"),Map->GetMarkers().ContainsByPredicate([](const auto& M){return M.Id==FName(TEXT("BrokenCoast.Uplink"))&&M.bObjective;}));
 TestFalse(TEXT("Remote pointer cannot start transmission"),Uplink->TryInteract(Player));
 Player->SetActorLocation(Approach);
 auto* Wall=World->SpawnActor<AActor>();auto* WallBody=NewObject<UBoxComponent>(Wall);Wall->AddInstanceComponent(WallBody);Wall->SetRootComponent(WallBody);
 WallBody->SetBoxExtent(FVector(10,80,140));WallBody->SetCollisionObjectType(ECC_WorldStatic);WallBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);WallBody->SetCollisionResponseToAllChannels(ECR_Block);WallBody->RegisterComponent();Wall->SetActorLocation((Approach+Uplink->GetActorLocation())*.5f);
 TestFalse(TEXT("Actual wall refuses start"),Uplink->TryInteract(Player));Wall->SetActorLocation(Approach+FVector(0,3000,0));
 Player->SetRole(ROLE_AutonomousProxy);TestFalse(TEXT("Non-authority player cannot start"),Uplink->TryInteract(Player));Player->SetRole(ROLE_Authority);
 TestTrue(TEXT("Ordinary nearby NPC search reaches real uplink"),Player->FindNearbyNPC()==Uplink);
 if(!TestTrue(TEXT("Nearby authoritative interaction starts actual clock"),Uplink->TryInteract(Player)))return false;
 TestFalse(TEXT("Repeated interaction cannot reset active clock"),Uplink->TryInteract(Player));
 auto Advance=[&](float Seconds){for(int32 I=0;I<FMath::CeilToInt(Seconds/.05f);++I){++GFrameCounter;World->Tick(LEVELTICK_All,.05f);}};
 Advance(1.f);
 TestTrue(TEXT("World time advances real transmission"),Uplink->GetRemainingSeconds()>0&&Uplink->GetRemainingSeconds()<Uplink->TransmissionSeconds);
 TestTrue(TEXT("Tracker exposes cancellation condition"),Map->GetCampaignObjective().ToString().Contains(TEXT("leaving or dying resets")));
 Player->SetActorLocation(Uplink->GetActorLocation()+FVector(-Uplink->WorkingRadius-100,0,0));Advance(.05f);
 TestFalse(TEXT("Leaving working radius cancels"),Uplink->IsTransmitting());TestEqual(TEXT("Leaving discards partial progress"),Uplink->GetRemainingSeconds(),0.f);
 Player->SetActorLocation(Approach);TestTrue(TEXT("Returning permits a fresh attempt"),Uplink->TryInteract(Player));
 Wall->SetActorLocation((Approach+Uplink->GetActorLocation())*.5f);Advance(.05f);
 TestFalse(TEXT("Losing static sightline cancels transmission"),Uplink->IsTransmitting());Wall->Destroy();
 TestTrue(TEXT("Clear approach restarts"),Uplink->TryInteract(Player));Advance(.1f);
 FBreakerDamageRequest Death;Death.BaseDamage=Player->GetCombat()->GetMaxHealth()*2;Death.DamageFamily=EBreakerDamageFamily::TrueDamage;Death.bCanCritical=false;Death.bCanBeAvoided=false;Death.bBypassShield=true;
 Player->GetCombat()->ReceiveDamage(Death);
 TestTrue(TEXT("Native lethal hit actually kills player"),Player->GetCombat()->IsDead());
 TestFalse(TEXT("Real death callback cancels immediately"),Uplink->IsTransmitting());TestFalse(TEXT("Death cannot complete flag"),Uplink->IsCompleteFor(Player));
 TestFalse(TEXT("Dead player cannot restart"),Uplink->TryInteract(Player));Player->GetCombat()->RestoreVitals();
 TestTrue(TEXT("Revival permits new complete attempt"),Uplink->TryInteract(Player));
 Advance(Uplink->TransmissionSeconds-.1f);TestFalse(TEXT("Transmission cannot finish early"),Uplink->IsCompleteFor(Player));
 Advance(.2f);TestTrue(TEXT("Actual uninterrupted clock commits completion"),Uplink->IsCompleteFor(Player));
 TestFalse(TEXT("Completion stops ticking and releases ownership"),Uplink->IsTransmitting());TestFalse(TEXT("Completed interaction cannot repeat"),Uplink->TryInteract(Player));
 TestTrue(TEXT("Completion directs existing return path"),Map->GetCampaignObjective().ToString().Contains(TEXT("Return to Anchor 13")));
 TestFalse(TEXT("Completed objective marker is removed"),Map->GetMarkers().ContainsByPredicate([](const auto& M){return M.Id==FName(TEXT("BrokenCoast.Uplink"));}));
 auto* Written=NewObject<UBreakerSaveGame>();Written->QuestFlags=Journal->GetState().Flags;Written->QuestCounters=Journal->GetState().Counters;
 TArray<uint8> Bytes;if(!TestTrue(TEXT("Journal serializes through actual save archive"),UGameplayStatics::SaveGameToMemory(Written,Bytes)))return false;
 auto* Read=Cast<UBreakerSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));auto* Returning=SpawnPlayer();if(!Read||!Returning)return false;
 Returning->GetQuestJournal()->RestoreFrom(Read->QuestFlags,Read->QuestCounters);
 const FVector ConsoleAt=Uplink->GetActorLocation();Uplink->Destroy();
 auto* Revisited=World->SpawnActor<ABreakerCoastalUplink>(ConsoleAt,FRotator::ZeroRotator);if(!Revisited)return false;Revisited->DispatchBeginPlay();Returning->SetActorLocation(Approach);
 TestTrue(TEXT("Recreated console reads restored persistent completion"),Revisited->IsCompleteFor(Returning));
 TestFalse(TEXT("Revisit cannot retrigger completed transmission"),Revisited->TryInteract(Returning));
 TestTrue(TEXT("Completed revisit has no empty interaction label"),Revisited->GetUplinkPrompt(Returning).IsEmpty());
 return true;
}
#endif
