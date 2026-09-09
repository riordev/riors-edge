#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Combat/BreakerEnemyPlateVisibility.h"
#include "Combat/BreakerBossEnemy.h"
#include "Combat/BreakerEnemy.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerEnemyPlateVisibilityRuntimeTest,"RiorsEdge.UI.EnemyPlate.NativeOcclusion",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBreakerEnemyPlateVisibilityRuntimeTest::RunTest(const FString&)
{
 UWorld::InitializationValues Init;Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);if(!World)return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Boss=World->SpawnActor<ABreakerBossEnemy>(FVector(1000,0,100),FRotator::ZeroRotator,Spawn);
 auto* Trash=World->SpawnActor<ABreakerEnemy>(FVector(1000,100,100),FRotator::ZeroRotator,Spawn);
 auto* Wall=World->SpawnActor<AActor>();auto* Viewer=World->SpawnActor<AActor>();if(!Boss||!Trash||!Wall||!Viewer)return false;
 auto* Box=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(Box);Wall->AddInstanceComponent(Box);
 Box->SetBoxExtent(FVector(50,500,500));Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();
 Wall->SetActorLocation(FVector(500,1000,200));
 const FVector Eye(0,0,200);
 auto Visible=[&](ABreakerEnemy* Enemy){const auto* Capsule=Enemy->FindComponentByClass<UCapsuleComponent>();
   return BreakerEnemyPlateVisibility::IsVisible(World,Eye,Enemy->GetActorLocation()+FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()),Enemy,Viewer);};
 TestEqual(TEXT("Native boss rank witness"),Boss->GetMonsterRank(),EBreakerMonsterRank::Boss);
 TestTrue(TEXT("Unoccluded native boss plate is eligible"),Visible(Boss));
 TestTrue(TEXT("Unoccluded ordinary plate is eligible"),Visible(Trash));
 Wall->SetActorLocation(FVector(500,0,200));
 TestFalse(TEXT("World static wall hides whole boss plate"),Visible(Boss));
 TestFalse(TEXT("Same wall hides ordinary plate"),Visible(Trash));
 Box->SetCollisionObjectType(ECC_WorldDynamic);
 TestTrue(TEXT("Dynamic crowd body does not hide boss plate"),Visible(Boss));
 Box->SetCollisionObjectType(ECC_WorldStatic);Wall->Destroy();
 TestTrue(TEXT("Removing cover restores boss eligibility"),Visible(Boss));
 return true;
}
#endif
