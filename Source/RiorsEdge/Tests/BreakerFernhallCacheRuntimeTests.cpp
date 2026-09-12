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
#include "Interaction/BreakerSupplyChest.h"
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
        TestFalse(TEXT("Distant cache cannot become the focused prompt"),Player->FindNearbyNPC()==Cache);
        Player->SetActorLocation(Cache->GetActorLocation()+Cache->GetActorForwardVector()*180.f);
        auto* Wall=World->SpawnActor<AActor>();if(!Wall)return false;
        auto* WallBody=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(WallBody);Wall->AddInstanceComponent(WallBody);
        WallBody->SetBoxExtent(FVector(30,30,100));WallBody->SetCollisionObjectType(ECC_WorldStatic);
        WallBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);WallBody->SetCollisionResponseToAllChannels(ECR_Block);WallBody->RegisterComponent();
        Wall->SetActorLocation((Player->GetActorLocation()+Cache->GetActorLocation())*.5f);
        TestFalse(TEXT("Authoritative pointer interaction cannot open through static cover"),Cache->TryOpen(Player));
        TestFalse(TEXT("Static cover also hides the focused cache prompt"),Player->FindNearbyNPC()==Cache);
        TestFalse(TEXT("Blocked attempt does not consume the reward"),Cache->IsOpened());
        Wall->Destroy();
        TestTrue(TEXT("Cleared nearby cache regains its focused prompt after cover removal"),Player->FindNearbyNPC()==Cache);
        TestEqual(TEXT("Cleared prompt remains the actual open verb"),Cache->GetCachePrompt().ToString(),FString(TEXT("OPEN CACHE")));
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

        // ---- SUPPLY CHESTS, IN THE SAME SHIPPED WORLD --------------------
        // The pure rules are proved on bare seeds in
        // RiorsEdge.Items.SupplyChest.Contents; what needs a world is that the
        // shipped Fernhall actually PLACES them, that they reach the F key
        // through the same NPC search the cache uses, and that each of the two
        // payouts really lands. A chest that rolls onto no walkable floor is
        // simply not there, so the count is a FLOOR rather than a figure.
        TArray<ABreakerSupplyChest*> Chests;
        for (TActorIterator<ABreakerSupplyChest> It(World); It; ++It) Chests.Add(*It);
        AddInfo(FString::Printf(TEXT("SUPPLY CHESTS  %d placed across three yards"), Chests.Num()));
        if (!TestTrue(TEXT("Shipping Fernhall places supply chests"), Chests.Num() >= 3)) return false;
        if (auto* Map = Player->FindComponentByClass<UBreakerLocalMapComponent>())
            for (const auto& Marker : Map->GetMarkers())
                for (const ABreakerSupplyChest* Chest : Chests)
                    TestFalse(TEXT("A chest is found by walking, not by a map pin"),
                        Marker.Location.Equals(Chest->GetActorLocation()));

        // ONE OF EACH KIND is no longer a thing: under O275 every chest pays
        // the currency floor AND one item at the completion floor. Open the
        // first chest the probe reaches and assert both.
        int32 Opened = 0;
        for (ABreakerSupplyChest* Chest : Chests)
        {
            if (Opened) break;
            Player->SetActorLocation(Chest->GetActorLocation() + Chest->GetActorForwardVector() * 180.0f);
            // THE SEARCH RETURNS THE NEAREST INTERACTABLE, and where a chest
            // lands is rolled per session: two of them can come down within a
            // search radius of each other, and then standing in front of one
            // finds the other. That is the search behaving correctly and this
            // assertion losing a coin flip — it went red once in a suite run and
            // green on the next with nothing changed.
            //
            // So a chest the probe does not reach is SKIPPED and said out loud,
            // rather than failing; what is asserted below the loop is that the
            // search reaches a chest at all, which is the claim this test is
            // actually making about the F key.
            if (Player->FindNearbyNPC() != Chest)
            {
                AddInfo(TEXT("SUPPLY CHESTS  a chest had a nearer neighbour at the probe; trying the next"));
                continue;
            }
            TestEqual(TEXT("An unopened chest names its verb"),
                Chest->GetChestPrompt().ToString(), FString(TEXT("OPEN CHEST")));
            TestFalse(TEXT("Null interactor refused"), Chest->TryOpen(nullptr));
            const int32 WalletBefore = Player->GetEquipment()->GetForgeWallet().Get();
            int32 PickupsBefore = 0;
            for (TActorIterator<ABreakerLootPickup> It(World); It; ++It) ++PickupsBefore;
            if (!TestTrue(TEXT("An unguarded chest opens on sight"), Chest->TryOpen(Player))) return false;
            const int32 WalletAfter = Player->GetEquipment()->GetForgeWallet().Get();
            int32 PickupsAfter = 0;
            for (TActorIterator<ABreakerLootPickup> It(World); It; ++It) ++PickupsAfter;
            ++Opened;
            TestTrue(TEXT("A chest credits Riftglass (O275)"), WalletAfter > WalletBefore);
            TestEqual(TEXT("and drops exactly one item (O275)"), PickupsAfter, PickupsBefore + 1);
            // ONE PAYOUT PER CHEST.
            TestFalse(TEXT("A chest cannot pay twice"), Chest->TryOpen(Player));
            TestTrue(TEXT("An opened chest drops its prompt"), Chest->GetChestPrompt().IsEmpty());
            TestFalse(TEXT("and stops capturing the F key"), Player->FindNearbyNPC() == Chest);
        }
        AddInfo(FString::Printf(TEXT("SUPPLY CHESTS  opened %d"), Opened));
        TestTrue(TEXT("Shipping NPC search reaches a chest"), Opened > 0);
    }
    return true;
}
#endif
