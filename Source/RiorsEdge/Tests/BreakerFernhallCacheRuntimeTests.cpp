#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Combat/BreakerSkirmisherEnemy.h"
#include "Combat/BreakerWardenEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerGameMode.h"
#include "Game/BreakerLocalMapComponent.h"
#include "Game/BreakerZoneBuilder.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/BreakerFeedstockPickup.h"
#include "Interaction/BreakerFernhallCache.h"
#include "Items/BreakerLootPickup.h"
#include "Items/BreakerDropTable.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Save/BreakerAccountSave.h"
#include "Save/BreakerQuestJournal.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerFernhallCacheRuntimeTest,
    "RiorsEdge.Campaign.FernhallCacheRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFernhallCacheRuntimeTest::RunTest(const FString& Parameters)
{
    UBreakerAccountSave* Account = NewObject<UBreakerAccountSave>();
    Account->bNeverPersist = true;
    UBreakerAccountSave::InjectForTesting(Account);
    ON_SCOPE_EXIT { UBreakerAccountSave::ResetCacheForTesting(); };
    // Native outdoor kills feed the same earned turn-in reward binding as play.
    for (int32 Visit = 0; Visit < 1; ++Visit)
    {
        UWorld::InitializationValues Init;
        Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/FernhallOutdoor_%s/Lvl_Fernhall"),
            *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        Package->SetFlags(RF_Transient);
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("Lvl_Fernhall")), Package,
            true, ERHIFeatureLevel::Num, &Init);
        if (!TestNotNull(TEXT("Isolated outdoor world"), World)) return false;
        FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
        Context.SetCurrentWorld(World);
        ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
        UBreakerGameInstance* Session = NewObject<UBreakerGameInstance>();
        World->SetGameInstance(Session);
        Context.OwningGameInstance = Session;
        World->GetWorldSettings()->DefaultGameMode = ABreakerGameMode::StaticClass();
        if (!TestTrue(TEXT("Authority mode installed"), World->SetGameMode(FURL()))) return false;
        World->InitializeActorsForPlay(FURL());
        ABreakerGameMode* Mode = World->GetAuthGameMode<ABreakerGameMode>();
        if (!TestNotNull(TEXT("Game mode"), Mode)) return false;
        Mode->DispatchBeginPlay();
        ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
        APlayerController* Controller = World->SpawnActor<APlayerController>();
        if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Controller"), Controller)) return false;
        Controller->Possess(Player);
        // No Character BeginPlay: that routine owns save loading/persistence.
        // Bind the same reflected kill handler it binds, so actual combat
        // deaths reach real quest logic without duplicating its conditions.
        Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
        Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->BindAttributes(Player->GetAttributes());
        Player->GetEquipment()->BindAttributes(Player->GetAttributes());
        if (!TestNotNull(TEXT("Actual character quest kill handler exists"), Player->FindFunction(FName(TEXT("HandleQuestKill"))))) return false;
        FScriptDelegate QuestKill;
        QuestKill.BindUFunction(Player, FName(TEXT("HandleQuestKill")));
        Player->GetCombat()->OnKillDealt.Add(QuestKill);
        UBreakerQuestJournal* Journal = Player->GetQuestJournal();
        if (!TestNotNull(TEXT("Character constructs its quest journal before BeginPlay"), Journal)) return false;
        Player->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster);
        Player->BindQuestRewardEvents();
        Player->BindQuestRewardEvents(); // Rebinding cannot duplicate payment.
        Journal->SetFlag(TEXT("Quest.KessSalvage.Accepted"));
        Mode->HandleStartingNewPlayer_Implementation(Controller);
        TestFalse(TEXT("Ordinary Fernhall stays outside a rift"), Mode->IsRiftInstance());
        TestFalse(TEXT("Outdoor patrols do not activate the wave controller"), Mode->IsWaveActive());
        ABreakerFernhallCache* Cache=nullptr;int32 CacheCount=0;
        for(TActorIterator<ABreakerFernhallCache> It(World);It;++It){Cache=*It;++CacheCount;}
        if(!TestEqual(TEXT("Shipping Fernhall spawns one physical cache"),CacheCount,1)||!Cache)return false;
        if(auto* Map=Player->FindComponentByClass<UBreakerLocalMapComponent>())
            for(const auto& Marker:Map->GetMarkers())TestFalse(TEXT("Cache does not add a map marker"),Marker.Location.Equals(Cache->GetActorLocation()));
        TArray<ABreakerEnemy*> Pocket;
        for(TActorIterator<ABreakerEnemy> It(World);It;++It)
            if(It->ActorHasTag(TEXT("Fernhall.Outdoor.3")))Pocket.Add(*It);
        if(!TestTrue(TEXT("Cache guards are actual off-lane pocket members"),Pocket.Num()>1))return false;
        Player->SetActorLocation(Cache->GetActorLocation()+Cache->GetActorForwardVector()*180.f);
        TestTrue(TEXT("Shipping NPC search reaches cache"),Player->FindNearbyNPC()==Cache);
        TestFalse(TEXT("Living pocket blocks cache"),Cache->TryOpen(Player));
        TestFalse(TEXT("Null interactor refused"),Cache->TryOpen(nullptr));
        auto Kill=[&](ABreakerEnemy* Enemy)
        {
            if(!Enemy->HasActorBegunPlay())Enemy->DispatchBeginPlay();
            auto* Sink=Enemy->FindComponentByClass<UBreakerCombatComponent>();
            FBreakerDamageRequest Hit;Hit.BaseDamage=Sink->GetMaxHealth()*2;
            Hit.DamageFamily=EBreakerDamageFamily::TrueDamage;Hit.bCanCritical=false;Hit.bCanBeAvoided=false;Hit.bBypassShield=true;Hit.SetInstigator(Player);
            return Sink->ReceiveDamage(Hit).bKilled;
        };
        for(int32 Index=0;Index<Pocket.Num();++Index)
        {
            if(!TestTrue(TEXT("Real guarded-pocket kill"),Kill(Pocket[Index])))return false;
            if(Index+1<Pocket.Num())TestFalse(TEXT("Partial pocket clear never unlocks loot"),Cache->TryOpen(Player));
        }
        TestTrue(TEXT("All observed native deaths unlock cache"),Cache->IsPocketCleared());
        // A corpse disappearing after its observed death cannot relock the cache.
        Pocket[0]->Destroy();
        Player->SetActorLocation(Cache->GetActorLocation()+FVector(1000,0,0));
        TestFalse(TEXT("Distant interactor cannot claim reward"),Cache->TryOpen(Player));
        Player->SetActorLocation(Cache->GetActorLocation()+Cache->GetActorForwardVector()*180.f);
        auto* Wall=World->SpawnActor<AActor>();if(!Wall)return false;
        auto* WallBody=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(WallBody);Wall->AddInstanceComponent(WallBody);
        WallBody->SetBoxExtent(FVector(30,30,100));WallBody->SetCollisionObjectType(ECC_WorldStatic);
        WallBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);WallBody->SetCollisionResponseToAllChannels(ECR_Block);WallBody->RegisterComponent();
        Wall->SetActorLocation((Player->GetActorLocation()+Cache->GetActorLocation())*.5f);
        TestFalse(TEXT("Authoritative pointer interaction cannot open through static cover"),Cache->TryOpen(Player));
        TestFalse(TEXT("Blocked attempt does not consume the reward"),Cache->IsOpened());
        Wall->Destroy();
        TSet<ABreakerLootPickup*> Before;
        for(TActorIterator<ABreakerLootPickup> It(World);It;++It)Before.Add(*It);
        bool bReentryCalled=false;bool bReentryPaid=false;
        const auto SpawnHandle=World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([&](AActor* Spawned)
        {if(Cast<ABreakerLootPickup>(Spawned)){bReentryCalled=true;bReentryPaid=Cache->TryOpen(Player);}}));
        const bool bOpened=Cache->TryOpen(Player);World->RemoveOnActorSpawnedHandler(SpawnHandle);
        if(!TestTrue(TEXT("Actual cleared cache opens"),bOpened))return false;
        TestTrue(TEXT("Native pickup spawn invoked reentrant interaction"),bReentryCalled);
        TestFalse(TEXT("Claim precedes pickup spawn callback"),bReentryPaid);
        TestFalse(TEXT("Repeated interaction cannot pay twice"),Cache->TryOpen(Player));
        TestTrue(TEXT("Opened cache has no empty text or interaction prompt"),Cache->GetCachePrompt().IsEmpty());
        TestFalse(TEXT("Opened cache no longer captures NPC interaction"),Player->FindNearbyNPC()==Cache);
        ABreakerLootPickup* Reward=nullptr;int32 NewPickups=0;
        for(TActorIterator<ABreakerLootPickup> It(World);It;++It)if(!Before.Contains(*It)){Reward=*It;++NewPickups;}
        if(!TestEqual(TEXT("One open creates exactly one physical item"),NewPickups,1)||!Reward)return false;
        TestTrue(TEXT("Cache reward uses normal valid item authoring"),Reward->GetItem().IsValid());
        const int32 AreaLevel=UBreakerZoneBuilder::FernhallRiftFor(NAME_None).EffectiveAreaLevel();
        TestEqual(TEXT("Cache item uses fixed regional level"),Reward->GetItem().ItemLevel,AreaLevel);
        TestTrue(TEXT("Cache obeys ordinary regional rarity gates"),UBreakerDropTableLibrary::IsRarityUnlocked(Reward->GetItem().Rarity,AreaLevel,EBreakerMonsterRank::Trash,FBreakerDropTableParams()));
        const int32 BackpackBefore=Player->GetEquipment()->GetBackpack().Num();
        Player->SetActorLocation(Reward->GetActorLocation()+FVector(100,0,0));
        if(!TestTrue(TEXT("Real physical cache reward can be collected"),Reward->TryPickup(Player)))return false;
        TestEqual(TEXT("Physical pickup enters backpack once"),Player->GetEquipment()->GetBackpack().Num(),BackpackBefore+1);
        TestFalse(TEXT("Collected cache item cannot duplicate"),Reward->TryPickup(Player));
        FActorSpawnParameters IsolatedSpawn;IsolatedSpawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* MissingGuard=World->SpawnActor<ABreakerEnemy>(FVector(40000,0,100),FRotator::ZeroRotator,IsolatedSpawn);
        auto* PartialGuard=World->SpawnActor<ABreakerEnemy>(FVector(42000,0,100),FRotator::ZeroRotator,IsolatedSpawn);
        if(!MissingGuard||!PartialGuard)return false;
        MissingGuard->ConfigureCrowdProbe();MissingGuard->DispatchBeginPlay();
        PartialGuard->ConfigureCrowdProbe();PartialGuard->DispatchBeginPlay();
        auto* Partial=World->SpawnActor<ABreakerFernhallCache>(FVector(40000,2000,100),FRotator::ZeroRotator,IsolatedSpawn);
        auto* Missing=World->SpawnActor<ABreakerFernhallCache>(FVector(42000,2000,100),FRotator::ZeroRotator,IsolatedSpawn);
        auto* Empty=World->SpawnActor<ABreakerFernhallCache>(FVector(44000,2000,100),FRotator::ZeroRotator,IsolatedSpawn);
        auto* Duplicate=World->SpawnActor<ABreakerFernhallCache>(FVector(46000,2000,100),FRotator::ZeroRotator,IsolatedSpawn);
        if(!Partial||!Missing||!Empty||!Duplicate)return false;
        Partial->Configure(AreaLevel,{PartialGuard},2);
        Missing->Configure(AreaLevel,{MissingGuard},1);
        Empty->Configure(AreaLevel,{},0);
        Duplicate->Configure(AreaLevel,{PartialGuard,PartialGuard},2);
        if(!TestTrue(TEXT("Actually clear the partial spawn's sole member"),Kill(PartialGuard)))return false;
        MissingGuard->Destroy();
        Player->SetActorLocation(Partial->GetActorLocation());
        TestFalse(TEXT("Missing expected spawn remains blocked after all actual deaths"),Partial->TryOpen(Player));
        Player->SetActorLocation(Missing->GetActorLocation());
        TestFalse(TEXT("Unobserved disappearance is not a cleared pocket"),Missing->TryOpen(Player));
        Player->SetActorLocation(Empty->GetActorLocation());
        TestFalse(TEXT("Empty roster never awards loot"),Empty->TryOpen(Player));
        Player->SetActorLocation(Duplicate->GetActorLocation());
        TestFalse(TEXT("Duplicate roster member cannot stand in for a missing guard"),Duplicate->TryOpen(Player));
        Cache->Configure(AreaLevel,{},0);
        Player->SetActorLocation(Cache->GetActorLocation());
        TestFalse(TEXT("Reconfiguration cannot rearm a paid cache"),Cache->TryOpen(Player));
    }
    return true;
}
#endif
