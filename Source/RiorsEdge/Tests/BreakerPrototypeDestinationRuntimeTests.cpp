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
#include "Game/BreakerContainmentHunt.h"
#include "Game/BreakerLocalMapComponent.h"
#include "Game/BreakerPrototypeDestinations.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/BreakerFernhallCache.h"
#include "Interaction/BreakerBasinRecorder.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootPickup.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Save/BreakerAccountSave.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerPrototypeDestinationPackagesTest,
    "RiorsEdge.Campaign.PrototypeDestinationPackages",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerPrototypeDestinationPackagesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Exactly three playable prototype definitions"),BreakerPrototypeDestinations::All().Num(),3);
    for (const auto& D : BreakerPrototypeDestinations::All())
    {
        if (!TestTrue(TEXT("Run create_prototype_destination_maps.py: actual destination World package exists"),BreakerPrototypeDestinations::HasMapPackage(D))) return false;
        FBreakerTravelDestination Entry;
        if (!TestTrue(TEXT("Actual package is exposed by the live travel registry"),ABreakerTravelPoint::FindDestination(D.Id,Entry))) return false;
        TestTrue(TEXT("Prototype is enabled general travel"),Entry.bEnabled && !Entry.bDoorOnly);
        TestTrue(TEXT("Player-facing description labels unfinished presentation honestly"),Entry.Description.Contains(TEXT("Prototype destination")));
        TestFalse(TEXT("Destination must not inherit gym content"),UBreakerGameInstance::IsGymMapName(D.MapName));
        TestEqual(TEXT("Three individually authored districts"),D.Districts.Num(),3);
        TestEqual(TEXT("Every district has its fixed regional level"),D.AreaLevels.Num(),D.Districts.Num());
    }
    FBreakerTravelDestination Entry;
    if (!TestTrue(TEXT("Historical Hub id still resolves"),ABreakerTravelPoint::FindDestination(TEXT("Hub"),Entry))) return false;
    TestEqual(TEXT("Hub display is Anchor 13 without changing identity"),Entry.DisplayName.ToString(),FString(TEXT("Anchor 13")));
    if (!TestTrue(TEXT("Historical Fernhall id still resolves"),ABreakerTravelPoint::FindDestination(TEXT("Fernhall"),Entry))) return false;
    TestEqual(TEXT("Fernhall display is the Approach"),Entry.DisplayName.ToString(),FString(TEXT("Fernhall Approach")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerPrototypeDestinationRuntimeTest,
    "RiorsEdge.Campaign.PrototypeDestinationRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerPrototypeDestinationRuntimeTest::RunTest(const FString& Parameters)
{
    auto* Account=NewObject<UBreakerAccountSave>(); Account->bNeverPersist=true;
    UBreakerAccountSave::InjectForTesting(Account);
    ON_SCOPE_EXIT { UBreakerAccountSave::ResetCacheForTesting(); };
    for (const auto& D : BreakerPrototypeDestinations::All())
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
        TArray<ABreakerEnemy*> Enemies; TArray<ABreakerFernhallCache*> Caches; TArray<ABreakerTravelPoint*> Gates;
        int32 Dressing=0;
        for (TActorIterator<AActor> It(World);It;++It)
        {
            if (auto* E=Cast<ABreakerEnemy>(*It)) Enemies.Add(E);
            if (auto* C=Cast<ABreakerFernhallCache>(*It)) Caches.Add(C);
            if (auto* G=Cast<ABreakerTravelPoint>(*It)) Gates.Add(G);
            if (It->ActorHasTag(TEXT("PrototypeDestination.Dressing"))) ++Dressing;
        }
        if (!TestEqual(TEXT("All eighteen actual fixed-region guards spawn on legal floor"),Enemies.Num(),18)) return false;
        if (!TestEqual(TEXT("Three real objective/reward caches"),Caches.Num(),3)) return false;
        if (!TestEqual(TEXT("Both ends have return travel"),Gates.Num(),2)) return false;
        TestTrue(TEXT("Existing authored environment kit is present"),Dressing>=6);
        // Setting is backed by actual landmarks, not only renamed travel text.
        // Existing capsule route, legal spawn and objective checks below remain.
        const TArray<FName> RequiredLandmarks=D.Id==TEXT("RedBasin")
            ? TArray<FName>{TEXT("Destination.Landmark.ScorchedFields"),TEXT("Destination.Landmark.BurnedHomestead"),TEXT("Destination.Landmark.ImpactCrater")}
            : D.Id==TEXT("StationZero")
                ? TArray<FName>{TEXT("Destination.Landmark.ResearchBench"),TEXT("Destination.Landmark.SpecimenGarden"),TEXT("Destination.Landmark.ContainmentLaboratory")}
                : TArray<FName>{TEXT("Destination.Landmark.DeparturesTerminal"),TEXT("Destination.Landmark.AircraftWreck"),TEXT("Destination.Landmark.MaintenanceHangar")};
        for (FName Landmark : RequiredLandmarks)
        {
            AActor* Found=nullptr;
            for (TActorIterator<AActor> It(World);It;++It) if(It->ActorHasTag(Landmark)){Found=*It;break;}
            TestNotNull(*FString::Printf(TEXT("Authored setting landmark exists: %s"),*Landmark.ToString()),Found);
        }
        if(D.Id==TEXT("PortMeridian"))
        {
            for(auto* Enemy:Enemies)
                TestFalse(TEXT("Airport guards never inherit Station Zero's priority hunt"),Enemy->ActorHasTag(UBreakerContainmentHunt::TargetTag()));
            TestFalse(TEXT("Airport objective is real supply recovery, not a research mission"),Player->FindComponentByClass<UBreakerLocalMapComponent>()->GetCampaignObjective().ToString().Contains(TEXT("Custodian")));
        }
        for (auto* Gate : Gates)
        {
            TestTrue(TEXT("Return gate is bound to actual travel handler"),Gate->OnDestinationSelected.IsBound());
            TestTrue(TEXT("A physical gate offers the hub return"),Gate->GetAvailableDestinations().ContainsByPredicate([](const auto& Place) { return Place.Id==ABreakerTravelPoint::HubDestinationId; }));
            TestFalse(TEXT("Gate does not offer the same destination"),Gate->GetAvailableDestinations().ContainsByPredicate([&](const auto& Place) { return Place.Id==D.Id; }));
        }
        for (auto* Enemy : Enemies)
            TestTrue(TEXT("Arrival begins beyond every actual enemy detection range"),
                FVector::Dist2D(Player->GetActorLocation(),Enemy->GetActorLocation())>Enemy->GetDetectionRange());
        const int32 BeforeCount=Enemies.Num(); Mode->HandleStartingNewPlayer_Implementation(Controller);
        int32 AfterCount=0; for (TActorIterator<ABreakerEnemy> It(World);It;++It) ++AfterCount;
        TestEqual(TEXT("Repeated arrival does not duplicate encounters"),AfterCount,BeforeCount);
        auto* Map=Player->FindComponentByClass<UBreakerLocalMapComponent>(); if (!Map) return false;
        TestTrue(TEXT("Local map shows actual floor footprints"),Map->GetGround().Num()>=5);
        TestEqual(TEXT("Local map names its real region"),Map->GetRegionName().ToString(),D.DisplayName);
        // Sweep the native standing capsule down the route through each district.
        // Enemy bodies are ignored; this proves geometry, not combat skill.
        FCollisionQueryParams Query(SCENE_QUERY_STAT(PrototypeWalkingRoute),false,Player);
        for (auto* Enemy : Enemies) Query.AddIgnoredActor(Enemy);
        for (auto* Cache : Caches) Query.AddIgnoredActor(Cache);
        for (auto* Gate : Gates) Query.AddIgnoredActor(Gate);
        FVector Previous=Player->GetActorLocation();
        const auto* Capsule=Player->GetCapsuleComponent();
        const float Half=Capsule->GetScaledCapsuleHalfHeight(),Radius=Capsule->GetScaledCapsuleRadius();
        for (const FVector Centre : D.Districts)
        {
            const FVector End=Centre+FVector(0,0,Half+3);
            Previous.Z=Half+3;
            FHitResult Block;
            const bool bBlocked=World->SweepSingleByObjectType(Block,Previous,End,FQuat::Identity,
                FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(Radius,Half),Query);
            TestFalse(*FString::Printf(TEXT("%s route %s -> %s clear; obstruction=%s at%s"),*D.Id.ToString(),*Previous.ToString(),*End.ToString(),*GetNameSafe(Block.GetActor()),*Block.ImpactPoint.ToString()),bBlocked);
            for (int32 Sample=0;Sample<=20;++Sample)
            {
                const FVector At=FMath::Lerp(Previous,End,Sample/20.f); FHitResult Floor;
                if (!TestTrue(TEXT("Walking route has floor at every sample"),World->LineTraceSingleByObjectType(Floor,At,At-FVector(0,0,Half+20),FCollisionObjectQueryParams(ECC_WorldStatic),Query))) return false;
                TestTrue(TEXT("Route never requires a jump over a missing floor"),Floor.ImpactNormal.Z>.7f);
            }
            Previous=End;
        }
        if(D.Id==TEXT("PortMeridian"))
        {
            // Airport side access is tested with the same standing capsule;
            // the main route alone does not prove caches or gates are usable.
            TArray<TPair<FVector,FVector>> Spurs;
            for(int32 Pocket=0;Pocket<Caches.Num();++Pocket)
            {
                const FVector Centre=D.Districts[Pocket];
                const FName Site(*FString::Printf(TEXT("Destination.Site.PortMeridian.%d"),Pocket));
                auto* const* Cache=Caches.FindByPredicate([&](const auto* C){return C->ActorHasTag(Site);});
                if(!Cache)return false;
                Spurs.Add({Centre,(*Cache)->GetActorLocation()});
            }
            for(auto* Gate:Gates)
                Spurs.Add({Gate->GetActorLocation().X<0 ? BreakerPrototypeDestinations::ArrivalLocation() : D.Districts.Last(),Gate->GetActorLocation()});
            for(const auto& Spur:Spurs)
            {
                FVector From=Spur.Key,To=Spur.Value;From.Z=To.Z=Half+3;
                FHitResult Block;
                TestFalse(TEXT("Airport cache/return approach fits the real standing capsule"),World->SweepSingleByObjectType(Block,From,To,FQuat::Identity,
                    FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(Radius,Half),Query));
                for(int32 Sample=0;Sample<=10;++Sample)
                {
                    const FVector At=FMath::Lerp(From,To,Sample/10.f);FHitResult Floor;
                    TestTrue(TEXT("Airport cache/return approach has continuous floor"),World->LineTraceSingleByObjectType(Floor,At,At-FVector(0,0,Half+20),
                        FCollisionObjectQueryParams(ECC_WorldStatic),Query));
                }
            }
        }
        TArray<int32> FixedLevels;
        for (auto* Enemy : Enemies) FixedLevels.Add(Enemy->GetAreaLevel());
        Player->GetProgression()->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50,Player->GetProgression()->ExperienceCurve));
        for (int32 Index=0;Index<Enemies.Num();++Index)
            TestEqual(TEXT("Returning at higher character level does not scale regional guards"),Enemies[Index]->GetAreaLevel(),FixedLevels[Index]);
        for (int32 Pocket=0;Pocket<3;++Pocket)
        {
            const FName Site(*FString::Printf(TEXT("Destination.Site.%s.%d"),*D.Id.ToString(),Pocket));
            auto* const* Found=Caches.FindByPredicate([&](auto* C) { return C->ActorHasTag(Site); });
            if (!Found) return false; auto* Cache=*Found;
            TestFalse(TEXT("Prototype caches have no Fernhall-specific map tag"),Cache->ActorHasTag(TEXT("Fernhall.Cache")));
            TestTrue(TEXT("Cache has a stable real objective marker"),Map->GetMarkers().ContainsByPredicate([&](const auto& M) { return M.Id==Site && M.Location.Equals(Cache->GetActorLocation()); }));
            Player->SetActorLocation(Cache->GetActorLocation()+Cache->GetActorForwardVector()*180);
            TestFalse(TEXT("Live guards block reward interaction"),Cache->TryOpen(Player));
            const FName GuardTag(*FString::Printf(TEXT("Destination.%s.Pocket.%d"),*D.Id.ToString(),Pocket));
            int32 Killed=0;
            for (auto* Enemy : Enemies) if (Enemy->ActorHasTag(GuardTag))
            {
                TestEqual(TEXT("Pocket uses its authored fixed level"),Enemy->GetAreaLevel(),D.AreaLevels[Pocket]);
                if (!Enemy->HasActorBegunPlay()) Enemy->DispatchBeginPlay();
                auto* Sink=Enemy->FindComponentByClass<UBreakerCombatComponent>();
                // Structural kill-gate fixture, not a damage/parity benchmark.
                FBreakerDamageRequest Hit; Hit.BaseDamage=Sink->GetMaxHealth()*2; Hit.SetInstigator(Player);
                Hit.DamageFamily=EBreakerDamageFamily::TrueDamage; Hit.bCanCritical=false; Hit.bCanBeAvoided=false; Hit.bBypassShield=true;
                if (!TestTrue(TEXT("Actual native guard death pays the pocket gate"),Sink->ReceiveDamage(Hit).bKilled)) return false;
                ++Killed;
            }
            TestEqual(TEXT("Every expected member is required"),Killed,6);
            TSet<ABreakerLootPickup*> Before;
            for (TActorIterator<ABreakerLootPickup> It(World);It;++It) Before.Add(*It);
            if (!TestTrue(TEXT("Completed pocket allows physical supply recovery"),Cache->TryOpen(Player))) return false;
            TestFalse(TEXT("Repeat interaction never pays twice"),Cache->TryOpen(Player));
            ABreakerLootPickup* Reward=nullptr; int32 NewRewards=0;
            for (TActorIterator<ABreakerLootPickup> It(World);It;++It) if (!Before.Contains(*It)) { Reward=*It; ++NewRewards; }
            if (!TestEqual(TEXT("Each pocket pays one cache item"),NewRewards,1) || !Reward) return false;
            TestEqual(TEXT("Reward uses regional item level even for a level-fifty visitor"),Reward->GetItem().ItemLevel,D.AreaLevels[Pocket]);
            Player->SetActorLocation(Reward->GetActorLocation()+FVector(100,0,0));
            const int32 Items=Player->GetEquipment()->GetBackpack().Num();
            if (!TestTrue(TEXT("Actual reward pickup enters normal equipment"),Reward->TryPickup(Player))) return false;
            TestEqual(TEXT("Reward is retained in the player's backpack"),Player->GetEquipment()->GetBackpack().Num(),Items+1);
            TestFalse(TEXT("Completed cache marker retires"),Map->GetMarkers().ContainsByPredicate([&](const auto& M) { return M.Id==Site; }));
        }
        if(D.Id==TEXT("RedBasin"))
            for(bool bExtraction:{false,true})
                for(TActorIterator<ABreakerBasinRecorder> It(World);It;++It)
                    if(It->IsExtraction()==bExtraction)
                    {
                        Player->SetActorLocation(It->GetActorLocation()+FVector(-150,0,0));
                        if(!TestTrue(TEXT("Native recovery/extraction advances destination objective"),It->TryInteract(Player)))return false;
                    }
        TestTrue(TEXT("Completed local objective directs the player home"),Map->GetCampaignObjective().ToString().Contains(TEXT("Return to Anchor 13")));
        // Real campaign death must return to the same pre-BeginPlay start,
        // not the stale origin stored in older map shells.
        FBreakerDamageRequest Death; Death.BaseDamage=Player->GetCombat()->GetMaxHealth()*2;
        Death.DamageFamily=EBreakerDamageFamily::TrueDamage; Death.bCanCritical=false;
        Death.bCanBeAvoided=false; Death.bBypassShield=true;
        Player->GetCombat()->ReceiveDamage(Death);
        TestTrue(TEXT("Native lethal hit begins campaign respawn"),Player->GetCombat()->IsDead());
        const uint64 FrameBeforeRespawn=GFrameCounter;
        for (int32 Step=0;Step<500 && Player->GetCombat()->IsDead();++Step)
        { ++GFrameCounter; World->Tick(LEVELTICK_All,.01f); }
        GFrameCounter=FrameBeforeRespawn;
        TestFalse(TEXT("Normal campaign death restores the player"),Player->GetCombat()->IsDead());
        TestTrue(TEXT("Respawn remembers the safe approach before BeginPlay"),
            FVector::Dist2D(Player->GetActorLocation(),BreakerPrototypeDestinations::ArrivalLocation())<1.f);
    }
    return true;
}
#endif
