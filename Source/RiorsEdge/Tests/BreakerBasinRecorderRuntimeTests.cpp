#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerGameMode.h"
#include "Game/BreakerLocalMapComponent.h"
#include "Game/BreakerPrototypeDestinations.h"
#include "Interaction/BreakerBasinRecorder.h"
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerBasinRecorderRuntimeTest,
    "RiorsEdge.Campaign.BasinRecorderRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerBasinRecorderRuntimeTest::RunTest(const FString& Parameters)
{
    auto* Account=NewObject<UBreakerAccountSave>(); Account->bNeverPersist=true;
    UBreakerAccountSave::InjectForTesting(Account);
    ON_SCOPE_EXIT { UBreakerAccountSave::ResetCacheForTesting(); };
    const auto* Definition=BreakerPrototypeDestinations::Find(TEXT("RedBasin"));
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
        ABreakerBasinRecorder* Recovery=nullptr;ABreakerBasinRecorder* Extraction=nullptr;int32 Consoles=0;
        for(TActorIterator<ABreakerBasinRecorder> It(World);It;++It){++Consoles;if(It->IsExtraction())Extraction=*It;else Recovery=*It;}
        if(!TestEqual(TEXT("Both sequential district interactions actually spawn"),Consoles,2)||!Recovery||!Extraction)return false;
        // Placement contract uses native actor bodies, not a decorative proxy.
        for(auto* Console:{Recovery,Extraction})
        {
            auto* Capsule=Console->FindComponentByClass<UCapsuleComponent>();
            if(!TestNotNull(TEXT("Actual console body exists"),Capsule))return false;
            FCollisionQueryParams Geometry(SCENE_QUERY_STAT(BasinRecorderBodyClearance),false,Console);
            Geometry.AddIgnoredActor(Player);
            TestFalse(TEXT("Console body clears authored static crater/homestead geometry"),World->OverlapAnyTestByObjectType(
                Capsule->GetComponentLocation(),Capsule->GetComponentQuat(),FCollisionObjectQueryParams(ECC_WorldStatic),
                FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Geometry));
            const FVector Approach=Console->GetActorLocation()+FVector(-150,0,0);
            FHitResult Obstruction;
            TestFalse(TEXT("Authored console approach has clear static sightline"),World->LineTraceSingleByObjectType(
                Obstruction,Approach,Console->GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic),Geometry));
            const auto* PlayerBody=Player->GetCapsuleComponent();
            TestFalse(TEXT("Standing player fits at console approach"),World->OverlapAnyTestByObjectType(
                Approach,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),
                FCollisionShape::MakeCapsule(PlayerBody->GetScaledCapsuleRadius(),PlayerBody->GetScaledCapsuleHalfHeight()),Geometry));
        }
        auto* Journal=Player->GetQuestJournal();auto* Map=Player->FindComponentByClass<UBreakerLocalMapComponent>();if(!Journal||!Map)return false;
        const int32 Items=Player->GetEquipment()->GetBackpack().Num(),XP=Player->GetProgression()->GetTotalExperience();
        TestTrue(TEXT("Recovery flag registered"),UBreakerQuestLibrary::GetRegisteredFlags().Contains(ABreakerBasinRecorder::RecoveredFlag()));
        TestTrue(TEXT("Extraction flag registered"),UBreakerQuestLibrary::GetRegisteredFlags().Contains(ABreakerBasinRecorder::ExtractedFlag()));
        TestFalse(TEXT("Remote pointer cannot recover"),Recovery->TryInteract(Player));
        Player->SetActorLocation(Extraction->GetActorLocation()+FVector(-150,0,0));
        TestFalse(TEXT("Extraction cannot skip recovery"),Extraction->TryInteract(Player));
        TestTrue(TEXT("Initial objective describes real recovery"),Map->GetCampaignObjective().ToString().Contains(TEXT("Recover the survey recorder")));
        Player->SetActorLocation(Recovery->GetActorLocation()+FVector(-150,0,0));
        auto* Wall=World->SpawnActor<AActor>();auto* Body=NewObject<UBoxComponent>(Wall);Wall->AddInstanceComponent(Body);Wall->SetRootComponent(Body);
        Body->SetBoxExtent(FVector(10,80,140));Body->SetCollisionObjectType(ECC_WorldStatic);Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Body->SetCollisionResponseToAllChannels(ECR_Block);Body->RegisterComponent();
        Wall->SetActorLocation((Recovery->GetActorLocation()+Player->GetActorLocation())*.5f);
        TestFalse(TEXT("Authoritative static-wall LOS refuses recovery"),Recovery->TryInteract(Player));Wall->Destroy();
        FBreakerDamageRequest Death;Death.BaseDamage=Player->GetCombat()->GetMaxHealth()*2;Death.DamageFamily=EBreakerDamageFamily::TrueDamage;Death.bCanCritical=false;Death.bCanBeAvoided=false;Death.bBypassShield=true;
        Player->GetCombat()->ReceiveDamage(Death);
        TestFalse(TEXT("Dead player cannot claim recorder"),Recovery->TryInteract(Player));
        Player->GetCombat()->RestoreVitals();
        TestTrue(TEXT("Actual nearby console is selected by ordinary interaction search"),Player->FindNearbyNPC()==Recovery);
        if(!TestTrue(TEXT("Recovery commits actual receipt"),Recovery->TryInteract(Player)))return false;
        TestFalse(TEXT("Recovery receipt is once-only"),Recovery->TryInteract(Player));
        TestTrue(TEXT("Carried objective advances to distinct final district"),Map->GetCampaignObjective().ToString().Contains(TEXT("Carry the survey recorder")));
        auto ArchiveRestore=[&]()
        {
            auto* Written=NewObject<UBreakerSaveGame>();Written->QuestFlags=Journal->GetState().Flags;Written->QuestCounters=Journal->GetState().Counters;
            TArray<uint8> Bytes;if(!UGameplayStatics::SaveGameToMemory(Written,Bytes))return false;
            auto* Read=Cast<UBreakerSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));if(!Read)return false;
            Journal->RestoreFrom(Read->QuestFlags,Read->QuestCounters);return true;
        };
        if(!TestTrue(TEXT("Carried recorder survives actual in-memory archive"),ArchiveRestore()))return false;
        TestFalse(TEXT("Reload does not reopen recovery"),Recovery->IsCurrentStep(Player));TestTrue(TEXT("Reload preserves extraction step"),Extraction->IsCurrentStep(Player));
        Player->SetActorLocation(Extraction->GetActorLocation()+FVector(-150,0,0));
        if(!TestTrue(TEXT("Second district relay accepts actual carried recorder"),Extraction->TryInteract(Player)))return false;
        TestFalse(TEXT("Extraction receipt cannot repeat"),Extraction->TryInteract(Player));
        if(!TestTrue(TEXT("Completed sequence survives archive"),ArchiveRestore()))return false;
        TestTrue(TEXT("Persisted completion directs real return"),Map->GetCampaignObjective().ToString().Contains(TEXT("Return to Anchor 13")));
        TestFalse(TEXT("Completed sequence has no stale objective markers"),Map->GetMarkers().ContainsByPredicate([](const auto& M){return M.Id==FName(TEXT("RedBasin.Recovery"))||M.Id==FName(TEXT("RedBasin.Extraction"));}));
        TestEqual(TEXT("Objective never invents backpack loot"),Player->GetEquipment()->GetBackpack().Num(),Items);TestEqual(TEXT("Objective never mints extra XP"),Player->GetProgression()->GetTotalExperience(),XP);
        int32 Gates=0;for(TActorIterator<ABreakerTravelPoint> It(World);It;++It){++Gates;TestTrue(TEXT("Existing return interaction remains bound"),It->OnDestinationSelected.IsBound());}
        TestEqual(TEXT("Both original return gates remain"),Gates,2);
    }
    return true;
}
#endif
