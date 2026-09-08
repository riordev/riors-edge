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
