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
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Save/BreakerAccountSave.h"
#include "Save/BreakerQuestJournal.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerFirstContractProgressionRuntimeTest,
    "RiorsEdge.Campaign.FirstContractProgressionRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFirstContractProgressionRuntimeTest::RunTest(const FString& Parameters)
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
        Journal->SetFlag(TEXT("Quest.FirstContract.Accepted"));
        Mode->HandleStartingNewPlayer_Implementation(Controller);
        TestFalse(TEXT("Ordinary Fernhall stays outside a rift"), Mode->IsRiftInstance());
        TestFalse(TEXT("Outdoor patrols do not activate the wave controller"), Mode->IsWaveActive());
        int32 Kills = 0;
        const int32 TokensBefore = Player->GetProgression()->GetUnspentAbilityTokens();
        const int32 CoreBefore = Player->GetProgression()->GetProgressionState().UnspentCorePoints;
        const int32 ItemsBefore = Player->GetEquipment()->GetBackpack().Num();
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            ABreakerEnemy* Enemy = *It;
            if (!Enemy->Tags.Contains(TEXT("Fernhall.Outdoor.0")) && !Enemy->Tags.Contains(TEXT("Fernhall.Outdoor.1"))) continue;
            Enemy->DispatchBeginPlay();
            // Combat-death/progression integration fixture, not a combat balance claim.
            FBreakerDamageRequest Kill;
            Kill.BaseDamage = 1000000; Kill.bCanCritical = false; Kill.bBypassShield = true; Kill.SetInstigator(Player);
            if (!TestTrue(TEXT("Actual entry enemy dies"), Enemy->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Kill).bKilled)) return false;
            ++Kills;
        }
        TestEqual(TEXT("Full first-contract route"), Kills, 7);
        auto* Progression = Player->GetProgression();
        TestEqual(TEXT("Natural outdoor XP before turn-in"), Progression->GetProgressionState().TotalExperience, 159);
        TestEqual(TEXT("Kills alone leave starter at level one"), Progression->GetProgressionState().CharacterLevel, 1);
        TestTrue(TEXT("Actual kills complete both contract objectives"), Journal->HasFlag(TEXT("Quest.FirstContract.SpillThinned")) && Journal->HasFlag(TEXT("Quest.FirstContract.EliteDown")));
        Player->AddQuestFlag(TEXT("Quest.FirstContract.TurnedIn"));
        TestEqual(TEXT("Earned authored turn-in adds 120 XP"), Progression->GetProgressionState().TotalExperience, 279);
        TestEqual(TEXT("First contract earns level two"), Progression->GetProgressionState().CharacterLevel, 2);
        TestTrue(TEXT("Ordinary level entitlement grants a Core point"), Progression->GetProgressionState().UnspentCorePoints > CoreBefore);
        TestEqual(TEXT("Level two does not bypass the authored level-five ability token gate"), Progression->GetUnspentAbilityTokens(), TokensBefore);
        TestEqual(TEXT("Existing item reward still pays once"), Player->GetEquipment()->GetBackpack().Num(), ItemsBefore + 1);
        Player->AddQuestFlag(TEXT("Quest.FirstContract.TurnedIn"));
        const TArray<FName> SavedFlags = Journal->GetFlags();
        const TMap<FName, int32> SavedCounters = Journal->GetState().Counters;
        Journal->RestoreFrom(SavedFlags, SavedCounters);
        Player->BindQuestRewardEvents();
        Player->AddQuestFlag(TEXT("Quest.FirstContract.TurnedIn"));
        TestEqual(TEXT("Duplicate and restored completed journal cannot repay XP"), Progression->GetProgressionState().TotalExperience, 279);
        TestEqual(TEXT("Duplicate and restore cannot repay item"), Player->GetEquipment()->GetBackpack().Num(), ItemsBefore + 1);
        ABreakerCharacter* Unearned = World->SpawnActor<ABreakerCharacter>();
        if (!TestNotNull(TEXT("Separate unearned contract subject"), Unearned)) return false;
        Unearned->BindQuestRewardEvents();
        Unearned->AddQuestFlag(TEXT("Quest.FirstContract.TurnedIn"));
        TestEqual(TEXT("Unearned turn-in pays no XP"), Unearned->GetProgression()->GetProgressionState().TotalExperience, 0);
        TestEqual(TEXT("Unearned turn-in pays no item"), Unearned->GetEquipment()->GetBackpack().Num(), 0);
    }
    return true;
}
#endif
