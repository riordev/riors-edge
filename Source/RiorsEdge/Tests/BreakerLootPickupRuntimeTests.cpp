#include "Tests/BreakerLootPickupRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Items/BreakerLootPickup.h"
#include "Items/BreakerLootLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void UBreakerLootPickupRuntimeObserver::Acquired(const FBreakerItemInstance& Item)
{
    ++Calls;
    if (Calls==1 && Pickup && Player) bReentryAccepted=Pickup->TryPickup(Player);
}
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerLootPickupRuntimeTest,"RiorsEdge.Items.LootPickup.ActualTransfer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerLootPickupRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Player=World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    auto* ASC=Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    auto* Equipment=Player->GetEquipment(); Equipment->BindAttributes(Player->GetAttributes());
    auto MakePickup=[&](int32 Seed)
    {
        auto* Pickup=World->SpawnActor<ABreakerLootPickup>(FVector(100,0,0),FRotator::ZeroRotator);
        if (Pickup) Pickup->SetItem(UBreakerLootLibrary::RollItem(TEXT("Pickup.Runtime"),EBreakerEquipSlot::Helmet,EBreakerItemRarity::Standard,1,Seed));
        return Pickup;
    };
    auto* Pickup=MakePickup(1);
    if (!Pickup) return false;
    Player->SetActorLocation(FVector(Pickup->GetInteractionRange()+200,0,0));
    TestFalse(TEXT("actual remote transfer refused"),Pickup->TryPickup(Player));
    Player->SetActorLocation(FVector::ZeroVector);
    auto* Wall=World->SpawnActor<AActor>();
    if (!Wall) return false;
    auto* Box=NewObject<UBoxComponent>(Wall);
    Wall->AddInstanceComponent(Box); Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(5,100,100)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
    Wall->SetActorLocation(FVector(50,0,0));
    TestFalse(TEXT("solid wall blocks pickup"),Pickup->TryPickup(Player));
    Wall->Destroy();
    FBreakerDamageRequest Kill;
    Kill.BaseDamage=Player->GetAttributes()->GetMaxHealth()+100;
    Kill.DamageFamily=EBreakerDamageFamily::TrueDamage; Kill.bCanCritical=false; Kill.bCanBeAvoided=false;
    Player->GetCombat()->ReceiveDamage(Kill);
    if (!TestTrue(TEXT("actual player death"),Player->GetCombat()->IsDead())) return false;
    TestFalse(TEXT("dead player cannot collect"),Pickup->TryPickup(Player));
    Player->GetCombat()->RestoreVitals();
    auto* Observer=NewObject<UBreakerLootPickupRuntimeObserver>(Player);
    Observer->Pickup=Pickup; Observer->Player=Player;
    Equipment->OnItemAcquired.AddDynamic(Observer,&UBreakerLootPickupRuntimeObserver::Acquired);
    const FGuid ItemId=Pickup->GetItem().ItemId;
    TestTrue(TEXT("actual rolled pickup transfers"),Pickup->TryPickup(Player));
    TestFalse(TEXT("acquisition callback cannot reenter transfer"),Observer->bReentryAccepted);
    TestEqual(TEXT("one acquisition callback"),Observer->Calls,1);
    TestEqual(TEXT("one backpack item"),Equipment->GetBackpack().Num(),1);
    TestTrue(TEXT("exact instance transferred"),Equipment->GetBackpack()[0].ItemId==ItemId);
    TestFalse(TEXT("repeated transfer refused"),Pickup->TryPickup(Player));
    Equipment->OnItemAcquired.RemoveDynamic(Observer,&UBreakerLootPickupRuntimeObserver::Acquired);
    // Capacity isolation uses ordinary rolled items, without changing capacity.
    for (int32 Index=1;Index<UBreakerEquipmentComponent::BackpackCapacity;++Index)
        if (!Equipment->AddToBackpack(UBreakerLootLibrary::RollItem(TEXT("Pickup.Capacity"),EBreakerEquipSlot::Helmet,EBreakerItemRarity::Standard,1,Index+10))) return false;
    auto* Full=MakePickup(500);
    if (!Full) return false;
    TestFalse(TEXT("full backpack refuses actual pickup"),Full->TryPickup(Player));
    TestFalse(TEXT("capacity refusal preserves grounded actor"),Full->IsActorBeingDestroyed());
    TestTrue(TEXT("capacity refusal releases transaction claim"),Full->CanPickup(Player));
    TestEqual(TEXT("capacity remains unchanged"),Equipment->GetBackpack().Num(),UBreakerEquipmentComponent::BackpackCapacity);
    return true;
}
#endif

// A DROP SITS ON THE GROUND. Spawned 128 cm up (an enemy's capsule centre
// plus the drop's own lift), it settles so the cube's underside rests a
// hair above the floor; spawned with no floor under it, it stays where it
// was put (the settle never invents a ground).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerLootPickupSettlesTest,"RiorsEdge.Items.LootPickup.SettlesOnFloor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerLootPickupSettlesTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Floor=World->SpawnActor<AActor>();
    if (!Floor) return false;
    auto* Slab=NewObject<UBoxComponent>(Floor);
    Floor->AddInstanceComponent(Slab); Floor->SetRootComponent(Slab);
    Slab->SetBoxExtent(FVector(1000,1000,10)); Slab->SetCollisionProfileName(TEXT("BlockAll")); Slab->RegisterComponent();
    Floor->SetActorLocation(FVector(0,0,-10));   // floor top at z 0
    // This fixture world never begins play, so the actor's BeginPlay — where
    // the settle lives — is dispatched by hand, as the cache runtime rig does.
    auto* Pickup=World->SpawnActor<ABreakerLootPickup>(FVector(100,0,128),FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("pickup spawns"),Pickup)) return false;
    if (!Pickup->HasActorBegunPlay()) Pickup->DispatchBeginPlay();
    const UStaticMeshComponent* Visual=Pickup->FindComponentByClass<UStaticMeshComponent>();
    if (!TestNotNull(TEXT("pickup has a visual"),Visual)) return false;
    const FBox Bounds=Visual->CalcBounds(Visual->GetComponentTransform()).GetBox();
    AddInfo(FString::Printf(TEXT("LOOT SETTLE  visual bottom %.1f top %.1f actor z %.1f"),Bounds.Min.Z,Bounds.Max.Z,Pickup->GetActorLocation().Z));
    TestTrue(TEXT("the cube's underside rests within 4 cm of the floor, not at chest height"),
        Bounds.Min.Z>=0.0f && Bounds.Min.Z<=4.0f);
    auto* Adrift=World->SpawnActor<ABreakerLootPickup>(FVector(5000,0,128),FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("adrift pickup spawns"),Adrift)) return false;
    if (!Adrift->HasActorBegunPlay()) Adrift->DispatchBeginPlay();
    TestEqual(TEXT("no floor under it: the drop stays where it was put"),static_cast<float>(Adrift->GetActorLocation().Z),128.0f,0.01f);
    return true;
}
