#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/BreakerFinaleEarthBuilder.h"
#include "Interaction/BreakerFinaleActor.h"
#include "Progression/BreakerProgressionComponent.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerFinaleGeometryTest, "RiorsEdge.Campaign.FinaleEarthGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerFinaleGeometryTest::RunTest(const FString& Parameters)
{
    for (bool bWon : {false,true})
    {
        UWorld::InitializationValues Init;
        Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        UWorld* World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
        if (!World) return false;
        ON_SCOPE_EXIT { World->DestroyWorld(false); };
        const FTransform Frame(FRotator(0,90,0),FVector(100,200,40));
        const auto Layout = bWon ? UBreakerFinaleEarthBuilder::BuildWon(World,Frame) : UBreakerFinaleEarthBuilder::BuildStripped(World,Frame);
        TestEqual(TEXT("Only stripped Earth has hostile pockets"), Layout.PocketCount,bWon ? 0 : 3);
        TestEqual(TEXT("Won Earth contains no combat roster"), Layout.Enemies.Num(),bWon ? 0 : 12);
        FVector Previous = Layout.PlayerArrival;
        for (FVector Point : Layout.Route)
        {
            FHitResult Hit;
            TestFalse(TEXT("Actual pawn capsule sweeps the complete civic avenue"),World->SweepSingleByChannel(
                Hit,Previous,Point,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88)));
            TestTrue(TEXT("Every route checkpoint has a walkable physical floor"),World->LineTraceSingleByChannel(
                Hit,Point,Point-FVector(0,0,160),ECC_Visibility) && Hit.ImpactNormal.Z>.9f);
            Previous=Point;
        }
        TestFalse(TEXT("Physical interaction location remains clear"),World->OverlapBlockingTestByChannel(
            Layout.InteractionLocation,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88)));
        for (const auto& Spawn : Layout.Enemies)
        {
            TestTrue(TEXT("Ordinary Vestige archetypes only"),Spawn.EnemyClass==ABreakerEnemy::StaticClass() || Spawn.EnemyClass==ABreakerRangedEnemy::StaticClass());
            TestFalse(TEXT("Every enemy capsule clears civic props"),World->OverlapBlockingTestByChannel(
                Spawn.Location,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(45,90)));
            FHitResult Hit;
            TestTrue(TEXT("Every finite enemy has actual supporting floor"),World->LineTraceSingleByChannel(
                Hit,Spawn.Location,Spawn.Location-FVector(0,0,180),ECC_Visibility) && Hit.ImpactNormal.Z>.9f);
        }
        int32 Backdrops=0;
        for(TActorIterator<AActor> It(World);It;++It)
            if(It->ActorHasTag(TEXT("FinaleEarth.Backdrop")))
            {
                ++Backdrops;
                if(const auto* Shape=Cast<UPrimitiveComponent>(It->GetRootComponent()))
                    TestEqual(TEXT("Distant horizon cannot become a navigation shortcut"),Shape->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
            }
        TestTrue(TEXT("Both Earths have visible distant terrain"),Backdrops>=25);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerFinaleInteractionTest, "RiorsEdge.Campaign.FinalePhysicalInteraction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerFinaleInteractionTest::RunTest(const FString& Parameters)
{
    for(int32 Case : {0,1,2})
    {
        const bool bDevice=Case!=0, bSeal=Case!=2;
        UWorld::InitializationValues Init;
        Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        const FString Map=bDevice ? TEXT("Lvl_Anchor") : TEXT("Lvl_StrippedEarth");
        UPackage* Package=CreatePackage(*FString::Printf(TEXT("/Temp/Finale_%s/%s"),*FGuid::NewGuid().ToString(),*Map));
        Package->SetFlags(RF_Transient);
        UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,FName(*Map),Package,true,ERHIFeatureLevel::Num,&Init);
        if(!World)return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());
        ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
        ABreakerCharacter* Player=World->SpawnActor<ABreakerCharacter>(FVector(150,0,100),FRotator::ZeroRotator);
        ABreakerFinaleActor* Actor=World->SpawnActor<ABreakerFinaleActor>(FVector(0,0,100),FRotator::ZeroRotator);
        if(!Player||!Actor)return false;
        Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player,Player);
        Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        Player->GetProgression()->BindAttributes(Player->GetAttributes());
        // Explicit component fixture setup only: no Character BeginPlay or owner save hooks.
        if(bDevice)Actor->ConfigureDevice();else Actor->ConfigureFragment(3);
        Actor->DispatchBeginPlay();
        TestNull(TEXT("Device and fragment never become combat targets"),Actor->FindComponentByClass<UBreakerCombatComponent>());
        auto* Journal=Player->GetQuestJournal();
        TestFalse(TEXT("Physical actor refuses missing campaign prerequisites"),bDevice?Actor->TryChooseFinale(Player,true):Actor->TryRecoverFragment(Player));
        Journal->SetFlag(TEXT("Quest.Survivor.TurnedIn"));Journal->SetFlag(TEXT("Quest.Finale.Offered"));
        Journal->SetFlag(TEXT("Quest.Finale.Accepted"));Journal->SetFlag(TEXT("Mission.Act3.Finale.Arrived"));
        if(!bDevice)
        {
            TestFalse(TEXT("Acceptance never substitutes for finite enemy deaths"),Actor->TryRecoverFragment(Player));
            Actor->SetPocketCleared(0);Actor->SetPocketCleared(1);
            TestFalse(TEXT("One uncleared pocket still blocks recovery"),Actor->TryRecoverFragment(Player));
            Actor->SetPocketCleared(2);
            Player->SetActorLocation(FVector(1000,0,100));
            TestFalse(TEXT("Cleared encounter still requires physical recovery"),Actor->TryRecoverFragment(Player));
            Player->SetActorLocation(FVector(150,0,100));
            TestTrue(TEXT("Actual mission encounter helper awards recovered fragment"),Actor->TryRecoverFragment(Player));
            TestTrue(TEXT("Recovered objective persisted in journal"),Journal->HasFlag(TEXT("Quest.Finale.FragmentRecovered")));
            TestFalse(TEXT("Repeated recovery cannot award twice"),Actor->TryRecoverFragment(Player));
        }
        else
        {
            for(FName Flag:{FName(TEXT("Quest.Finale.FragmentRecovered")),FName(TEXT("Quest.Finale.ReturnedWithFragment")),
                FName(TEXT("Quest.Finale.Reconstructed")),FName(TEXT("Quest.Finale.ArrivedWon")),FName(TEXT("Quest.Finale.MetAlternate")),
                FName(TEXT("Quest.Finale.ReturnedFromWon"))})Journal->SetFlag(Flag);
            TestFalse(TEXT("All story flags still require level fifty"),Actor->TryChooseFinale(Player,true));
            Player->GetProgression()->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(49,
                Player->GetProgression()->ExperienceCurve));
            TestEqual(TEXT("Boundary fixture reaches level forty-nine"),Player->GetProgression()->GetCharacterLevel(),49);
            TestFalse(TEXT("Level forty-nine cannot complete either ending"),Actor->TryChooseFinale(Player,bSeal));
            Player->GetProgression()->AwardExperience(MAX_int32/2);
            TestEqual(TEXT("Fixture explicitly reaches actual level cap"),Player->GetProgression()->GetCharacterLevel(),50);
            Player->SetActorLocation(FVector(1000,0,100));
            TestFalse(TEXT("Final choice requires standing at the device"),Actor->TryChooseFinale(Player,true));
            Player->SetActorLocation(FVector(150,0,100));
            int32 Saves=0;Journal->OnPersistRequested.AddLambda([&]{++Saves;});
            Journal->OnFlagSet.AddLambda([&](FName){TestTrue(TEXT("Every observer sees atomic choice and completion"),
                Journal->HasFlag(bSeal?TEXT("Quest.Finale.Seal"):TEXT("Quest.Finale.Hold"))&&Journal->HasFlag(TEXT("Quest.Finale.TurnedIn")));});
            TestTrue(TEXT("Eligible living player chooses either authored ending"),Actor->TryChooseFinale(Player,bSeal));
            TestEqual(TEXT("One atomic choice requests one save"),Saves,1);
            TestFalse(TEXT("Opposite choice cannot also be selected"),Actor->TryChooseFinale(Player,!bSeal));
            TestFalse(TEXT("Repeat completion is refused"),Actor->TryChooseFinale(Player,bSeal));
            TestFalse(TEXT("Unchosen branch stays absent"),Journal->HasFlag(bSeal?TEXT("Quest.Finale.Hold"):TEXT("Quest.Finale.Seal")));
        }
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerAlternatePhysicalTest, "RiorsEdge.Campaign.AlternatePhysicalMeeting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerAlternatePhysicalTest::RunTest(const FString& Parameters)
{
    for(bool bWinningMap : {false,true})
    {
        UWorld::InitializationValues Init;
        Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        const FString Map=bWinningMap?TEXT("Lvl_WinningEarth"):TEXT("Lvl_Anchor");
        UPackage* Package=CreatePackage(*FString::Printf(TEXT("/Temp/Alternate_%s/%s"),*FGuid::NewGuid().ToString(),*Map));
        Package->SetFlags(RF_Transient);
        UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,FName(*Map),Package,true,ERHIFeatureLevel::Num,&Init);
        if(!World)return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());
        ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
        ABreakerCharacter* Player=World->SpawnActor<ABreakerCharacter>(FVector(150,0,100),FRotator::ZeroRotator);
        ABreakerNPC* NPC=World->SpawnActor<ABreakerNPC>(FVector(0,0,100),FRotator::ZeroRotator);
        if(!Player||!NPC)return false;
        Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player,Player);
        Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
        Player->GetCombat()->BindAttributes(Player->GetAttributes());
        auto* Journal=Player->GetQuestJournal();
        // Explicit prerequisite fixture, not a claim that these beats were earned here.
        for(FName Flag:{FName(TEXT("Quest.Survivor.TurnedIn")),FName(TEXT("Quest.Finale.Offered")),
            FName(TEXT("Quest.Finale.Accepted")),FName(TEXT("Mission.Act3.Finale.Arrived")),
            FName(TEXT("Quest.Finale.FragmentRecovered")),FName(TEXT("Quest.Finale.ReturnedWithFragment")),
            FName(TEXT("Quest.Finale.Reconstructed")),FName(TEXT("Quest.Finale.ArrivedWon"))})Journal->SetFlag(Flag);
        TestFalse(TEXT("Generic nearby NPC cannot impersonate the alternate"),ABreakerFinaleActor::TryMeetAlternate(NPC,Player));
        NPC->Tags.Add(TEXT("AlternateSelf"));
        if(!bWinningMap)
        {
            TestFalse(TEXT("An alternate tag in Anchor cannot complete Won meeting"),ABreakerFinaleActor::TryMeetAlternate(NPC,Player));
            continue;
        }
        Player->SetActorLocation(FVector(1000,0,100));
        TestFalse(TEXT("Alternate meeting requires physical proximity"),ABreakerFinaleActor::TryMeetAlternate(NPC,Player));
        Player->SetActorLocation(FVector(150,0,100));
        TestNull(TEXT("Alternate is a living friendly NPC, never a combat target"),NPC->FindComponentByClass<UBreakerCombatComponent>());
        TestTrue(TEXT("Actual Won meeting completes its current authored beat"),ABreakerFinaleActor::TryMeetAlternate(NPC,Player));
        TestTrue(TEXT("MetAlternate was set by the physical helper"),Journal->HasFlag(TEXT("Quest.Finale.MetAlternate")));
        TestFalse(TEXT("Meeting cannot replay once its beat is complete"),ABreakerFinaleActor::TryMeetAlternate(NPC,Player));
    }
    return true;
}
#endif
