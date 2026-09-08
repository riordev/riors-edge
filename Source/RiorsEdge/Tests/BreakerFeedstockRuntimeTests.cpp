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
#include "Game/BreakerZoneBuilder.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/BreakerFeedstockPickup.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Save/BreakerAccountSave.h"
#include "Save/BreakerQuestJournal.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerFeedstockRuntimeTest,
    "RiorsEdge.Campaign.FeedstockRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFeedstockRuntimeTest::RunTest(const FString& Parameters)
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
        // Restore an old partial counter without changing its serialized identity.
        TMap<FName, int32> OldCounters; OldCounters.Add(TEXT("Quest.KessSalvage.Kills"), 3);
        const TArray<FName> OldFlags = Journal->GetFlags();
        Journal->RestoreFrom(OldFlags, OldCounters);
        TArray<ABreakerEnemy*> Victims;
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
            if (It->Tags.Contains(TEXT("Fernhall.Outdoor.0"))) Victims.Add(*It);
        if (!TestTrue(TEXT("Actual Vestige pocket exists"), Victims.Num() >= 3)) return false;
        const int32 BackpackBefore = Player->GetEquipment()->GetBackpack().Num();
        auto* Other = World->SpawnActor<ABreakerCharacter>();
        if (!Other) return false;
        for (int32 Index = 0; Index < 3; ++Index)
        {
            auto* Enemy = Victims[Index];
            Enemy->DispatchBeginPlay();
            FBreakerDamageRequest Kill; Kill.BaseDamage = 1000000; Kill.bCanCritical = false; Kill.bBypassShield = true; Kill.SetInstigator(Player);
            const auto Result = Enemy->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Kill);
            if (!TestTrue(TEXT("Actual Vestige death"), Result.bKilled)) return false;
            TestEqual(TEXT("Death does not collect residue"), Journal->GetState().GetCounter(TEXT("Quest.KessSalvage.Kills")), 3 + Index);
            ABreakerFeedstockPickup* Pickup = nullptr;
            int32 Count = 0;
            for (TActorIterator<ABreakerFeedstockPickup> It(World); It; ++It)
                if (!It->IsActorBeingDestroyed()) { Pickup = *It; ++Count; }
            if (!TestEqual(TEXT("One new physical drop for one death"), Count, 1) || !Pickup) return false;
            FBreakerHitContext Duplicate; Duplicate.Target = Enemy; Duplicate.Instigator = Player; Duplicate.Result = Result;
            TestNull(TEXT("Duplicate death context cannot create another residue"), ABreakerFeedstockPickup::SpawnForKill(Player, Duplicate));
            TestTrue(TEXT("Pickup is owner replicated"), Pickup->GetIsReplicated() && Pickup->GetOwner() == Player);
            Player->SetActorLocation(Pickup->GetActorLocation() + FVector(1000,0,100));
            TestFalse(TEXT("Remote collection refused"), Pickup->TryCollect(Player));
            Other->SetActorLocation(Pickup->GetActorLocation() + FVector(0,0,100));
            TestFalse(TEXT("Another player cannot collect owned residue"), Pickup->TryCollect(Other));
            Player->SetActorLocation(Pickup->GetActorLocation() + FVector(0,0,100));
            if (Index == 0)
            {
                auto* Wall = World->SpawnActor<AActor>();
                auto* Box = NewObject<UBoxComponent>(Wall);
                Wall->AddInstanceComponent(Box); Wall->SetRootComponent(Box);
                Box->SetBoxExtent(FVector(60,60,5));
                Box->SetCollisionObjectType(ECC_WorldStatic);
                Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                Box->SetCollisionResponseToAllChannels(ECR_Block);
                Box->RegisterComponent();
                Wall->SetActorLocation(Pickup->GetActorLocation() + FVector(0,0,50));
                TestFalse(TEXT("Solid cover blocks physical collection"), Pickup->TryCollect(Player));
                Wall->Destroy();
            }
            TestTrue(TEXT("Existing F candidate reaches owned residue"), Player->FindNearbyFeedstock() == Pickup);
            if (!TestTrue(TEXT("Physical nearby collection accepted"), Pickup->TryCollect(Player))) return false;
            TestFalse(TEXT("Collected residue cannot pay twice"), Pickup->TryCollect(Player));
            TestEqual(TEXT("One collection advances one existing counter"), Journal->GetState().GetCounter(TEXT("Quest.KessSalvage.Kills")), 4 + Index);
        }
        TestTrue(TEXT("Three collections finish restored three-of-six progress"), Journal->HasFlag(TEXT("Quest.KessSalvage.FeedstockTaken")));
        TestEqual(TEXT("Residue occupies no backpack slot"), Player->GetEquipment()->GetBackpack().Num(), BackpackBefore);
    }
    return true;
}
#endif