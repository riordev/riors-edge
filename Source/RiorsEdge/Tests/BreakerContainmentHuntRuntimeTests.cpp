#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerGameMode.h"
#include "Game/BreakerLocalMapComponent.h"
#include "Game/BreakerPrototypeDestinations.h"
#include "Game/BreakerContainmentHunt.h"
#include "Save/BreakerQuestContent.h"
#include "Save/BreakerSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/BreakerFernhallCache.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootPickup.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Save/BreakerAccountSave.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerContainmentHuntRuntimeTest,
    "RiorsEdge.Campaign.ContainmentHuntRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerContainmentHuntRuntimeTest::RunTest(const FString& Parameters)
{
    auto* Account=NewObject<UBreakerAccountSave>(); Account->bNeverPersist=true;
    UBreakerAccountSave::InjectForTesting(Account);
    ON_SCOPE_EXIT { UBreakerAccountSave::ResetCacheForTesting(); };
    const auto* Definition=BreakerPrototypeDestinations::Find(TEXT("StationZero"));
    if(!Definition)return false;
    const auto& D=*Definition;
    {
        UWorld::InitializationValues Init;
        Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        auto* Package=CreatePackage(*FString::Printf(TEXT("/Temp/Prototype_%s/%s"),*FGuid::NewGuid().ToString(EGuidFormats::Digits),*D.MapName));
        Package->SetFlags(RF_Transient);
        auto* World=UWorld::CreateWorld(EWorldType::Game,false,FName(*D.MapName),Package,true,ERHIFeatureLevel::Num,&Init);
        if (!World) return false;
        auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
        ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
        auto* Session=NewObject<UBreakerGameInstance>(); World->SetGameInstance(Session); Context.OwningGameInstance=Session;
        Session->ActiveCharacterId=FGuid::NewGuid(); // Isolate BeginPlay from any real legacy character slot.
        World->GetWorldSettings()->DefaultGameMode=ABreakerGameMode::StaticClass();
        if (!TestTrue(TEXT("Native mode installed"),World->SetGameMode(FURL()))) return false;
        World->InitializeActorsForPlay(FURL());
        auto* Mode=World->GetAuthGameMode<ABreakerGameMode>(); if (!Mode) return false;
        Mode->DispatchBeginPlay();
        AActor* Start=Mode->ChoosePlayerStart_Implementation(nullptr);
        if (!TestNotNull(TEXT("Native prototype start exists before pawn BeginPlay"),Start)) return false;
        TestTrue(TEXT("Start uses safe shared approach"),Start->GetActorLocation().Equals(BreakerPrototypeDestinations::ArrivalLocation()));
        auto* Player=World->SpawnActor<ABreakerCharacter>(Start->GetActorLocation(),Start->GetActorRotation()); auto* Controller=World->SpawnActor<APlayerController>();
        if (!Player || !Controller) return false;
        Controller->Possess(Player); Player->bRefuseSavesForPendingCharacter=true;
        auto* ASC=Player->GetAbilitySystemComponent(); ASC->InitAbilityActorInfo(Player,Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes()); Player->GetProgression()->BindAttributes(Player->GetAttributes());
        Player->GetEquipment()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Swift);
        Player->DispatchBeginPlay();
        Mode->HandleStartingNewPlayer_Implementation(Controller);
        TestTrue(TEXT("Actual world proves arrival identity"),UBreakerGameInstance::IsDestinationMap(World,D.Id));
        TestFalse(TEXT("Ordinary destination is not a Rift instance"),Mode->IsRiftInstance());
        TestFalse(TEXT("Finite regional encounters do not start gym waves"),Mode->IsWaveActive());
        ABreakerEnemy* Target=nullptr;ABreakerEnemy* Other=nullptr;int32 TargetCount=0;
        for(TActorIterator<ABreakerEnemy> It(World);It;++It)
        {
            if(It->ActorHasTag(UBreakerContainmentHunt::TargetTag())){Target=*It;++TargetCount;}
            else if(!Other)Other=*It;
        }
        if(!TestEqual(TEXT("One actually spawned authored priority target"),TargetCount,1)||!Target||!Other)return false;
        TestEqual(TEXT("Priority target preserves fixed regional level"),Target->GetAreaLevel(),28);
        auto* Journal=Player->GetQuestJournal();auto* Map=Player->FindComponentByClass<UBreakerLocalMapComponent>();
        if(!Journal||!Map)return false;
        TestTrue(TEXT("Completion identity is registered"),UBreakerQuestLibrary::GetRegisteredFlags().Contains(UBreakerContainmentHunt::CompletionFlag()));
        TestFalse(TEXT("Hunt begins incomplete"),Journal->HasFlag(UBreakerContainmentHunt::CompletionFlag()));
        TestTrue(TEXT("HUD presents a hunt rather than cache recovery"),Map->GetCampaignObjective().ToString().Contains(TEXT("Hunt the Containment Custodian")));
        TestTrue(TEXT("Real target location has objective marker"),Map->GetMarkers().ContainsByPredicate([&](const auto& M){return M.Id==UBreakerContainmentHunt::TargetTag()&&M.Location.Equals(Target->GetActorLocation());}));
        auto Kill=[&](ABreakerEnemy* Victim)
        {
            if(!Victim->HasActorBegunPlay())Victim->DispatchBeginPlay();
            auto* Sink=Victim->FindComponentByClass<UBreakerCombatComponent>();
            FBreakerDamageRequest Hit;Hit.BaseDamage=Sink->GetMaxHealth()*2;Hit.SetInstigator(Player);
            Hit.DamageFamily=EBreakerDamageFamily::TrueDamage;Hit.bCanCritical=false;Hit.bCanBeAvoided=false;Hit.bBypassShield=true;
            return Sink->ReceiveDamage(Hit);
        };
        // Lifecycle fixture deliberately supplies lethal accepted damage, not a
        // weapon balance claim; no manual death broadcast or completion setter.
        if(!TestTrue(TEXT("Ordinary guard accepts actual lethal damage"),Kill(Other).bKilled))return false;
        TestFalse(TEXT("Wrong enemy never advances the hunt"),Journal->HasFlag(UBreakerContainmentHunt::CompletionFlag()));
        const int32 BeforeXP=Player->GetProgression()->GetTotalExperience();
        if(!TestTrue(TEXT("Authored target accepts actual lethal damage"),Kill(Target).bKilled))return false;
        TestTrue(TEXT("Native target death commits completion"),Journal->HasFlag(UBreakerContainmentHunt::CompletionFlag()));
        TestTrue(TEXT("Ordinary native kill reward pays"),Player->GetProgression()->GetTotalExperience()>BeforeXP);
        const int32 AfterXP=Player->GetProgression()->GetTotalExperience(),FlagCount=Journal->GetFlags().Num();
        TestFalse(TEXT("Repeated lethal request cannot kill twice"),Kill(Target).bKilled);
        TestEqual(TEXT("Repeated death does not pay again"),Player->GetProgression()->GetTotalExperience(),AfterXP);
        TestEqual(TEXT("Completion is recorded once"),Journal->GetFlags().Num(),FlagCount);
        TestFalse(TEXT("Completed target marker retires"),Map->GetMarkers().ContainsByPredicate([](const auto& M){return M.Id==UBreakerContainmentHunt::TargetTag();}));
        TestTrue(TEXT("Hunt completion supplies real return objective"),Map->GetCampaignObjective().ToString().Contains(TEXT("Return to Anchor 13")));
        auto* Written=NewObject<UBreakerSaveGame>();
        Written->QuestFlags=Journal->GetState().Flags;Written->QuestCounters=Journal->GetState().Counters;
        TArray<uint8> Bytes;
        if(!TestTrue(TEXT("Completed hunt serializes through native in-memory save archive"),UGameplayStatics::SaveGameToMemory(Written,Bytes)))return false;
        auto* Read=Cast<UBreakerSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
        if(!TestNotNull(TEXT("Native archive reloads without player slot IO"),Read))return false;
        Journal->RestoreFrom(Read->QuestFlags,Read->QuestCounters);
        TestTrue(TEXT("Archived completion survives journal restore"),Journal->HasFlag(UBreakerContainmentHunt::CompletionFlag()));
        TestTrue(TEXT("Restored objective still directs return"),Map->GetCampaignObjective().ToString().Contains(TEXT("Return to Anchor 13")));
        TestFalse(TEXT("Restored completed hunt has no objective marker"),Map->GetMarkers().ContainsByPredicate([](const auto& M){return M.Id==UBreakerContainmentHunt::TargetTag();}));
        for(const auto& Marker:Map->GetMarkers())
            if(Marker.Id.ToString().StartsWith(TEXT("Destination.Site.StationZero.")))
                TestFalse(TEXT("Optional research lockers are not primary objectives"),Marker.bObjective);
        int32 Gates=0;
        for(TActorIterator<ABreakerTravelPoint> It(World);It;++It)
        {
            if(!It->ActorHasTag(TEXT("PrototypeDestination.ReturnGate")))continue;
            ++Gates;TestTrue(TEXT("Return interaction remains bound to native travel"),It->OnDestinationSelected.IsBound());
            TestTrue(TEXT("Actual gate offers Anchor"),It->GetAvailableDestinations().ContainsByPredicate([](const auto& Entry){return Entry.Id==ABreakerTravelPoint::HubDestinationId;}));
        }
        TestEqual(TEXT("Both real return interactions remain reachable"),Gates,2);
        int32 Opened=0;for(TActorIterator<ABreakerFernhallCache> It(World);It;++It)if(It->IsOpened())++Opened;
        TestEqual(TEXT("Hunt completion never auto-opens recovery caches"),Opened,0);
    }
    return true;
}
#endif
