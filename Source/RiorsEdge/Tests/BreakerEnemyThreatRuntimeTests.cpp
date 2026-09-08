#include "Tests/BreakerThreatRuntimeObserver.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDeployable.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerEnemyThreatMath.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/BreakerGameMode.h"
#include "Game/BreakerGameInstance.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
void UBreakerThreatRuntimeObserver::Observe(const FBreakerHitContext& Hit)
{
    ++Hits; LastRewardOwner = Hit.Instigator; LastThreatSource = Hit.ThreatSource.Get();
}
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerThreatRuntimeTest,"RiorsEdge.Combat.Threat.NativeTargeting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerThreatRuntimeTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Health and shield both earn threat"),BreakerEnemyThreat::Earned(4,6),10.0f);
    TestEqual(TEXT("No actual damage earns nothing"),BreakerEnemyThreat::Earned(0,0),0.0f);
    TestFalse(TEXT("Equal threat cannot displace current target just by distance"),
        BreakerEnemyThreat::Prefer(10,false,1,10,true,100));
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto SpawnPlayer=[&](FVector Location)
    {
        auto* P=World->SpawnActor<ABreakerCharacter>(Location,FRotator::ZeroRotator);
        if (P)
        {
            P->GetAbilitySystemComponent()->InitAbilityActorInfo(P,P);
            P->GetAbilitySystemComponent()->AddAttributeSetSubobject(P->GetAttributes());
            P->GetCombat()->BindAttributes(P->GetAttributes());
        }
        return P;
    };
    auto* Near=SpawnPlayer(FVector(500,0,100)); auto* Far=SpawnPlayer(FVector(900,0,100));
    auto* Enemy=World->SpawnActor<ABreakerEnemy>(FVector(0,0,100),FRotator::ZeroRotator);
    if (!Near || !Far || !Enemy) return false;
    Enemy->ConfigureCrowdProbe(); Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
    auto* Victim=Enemy->FindComponentByClass<UBreakerCombatComponent>();
    if (!Victim) return false;
    auto Select=[&]() { Enemy->Tick(0); return Enemy->GetThreatTarget(); };
    Enemy->SetRole(ROLE_SimulatedProxy);
    TestNull(TEXT("Client proxy does not author target selection"),Select());
    Enemy->SetRole(ROLE_Authority);
    TestTrue(TEXT("Zero-threat baseline acquires nearest player"),Select()==Near);
    auto* Observer=NewObject<UBreakerThreatRuntimeObserver>(Far);
    Far->GetCombat()->OnHitDealt.AddDynamic(Observer,&UBreakerThreatRuntimeObserver::Observe);
    auto Damage=[&](AActor* Owner,AActor* Direct,float Amount)
    {
        FBreakerDamageRequest Hit; Hit.BaseDamage=Amount; Hit.DamageFamily=EBreakerDamageFamily::TrueDamage;
        Hit.bCanCritical=false; Hit.bCanBeAvoided=false; Hit.SetInstigator(Owner); Hit.ThreatSource=Direct;
        return Victim->ReceiveDamage(Hit);
    };
    Damage(Far,nullptr,10);
    TestTrue(TEXT("Actual accepted hit draws target away from nearest"),Select()==Far);
    Damage(Near,nullptr,10);
    TestTrue(TEXT("Actual tied damage retains current target"),Select()==Far);
    auto* Turret=World->SpawnActor<ABreakerDeployable>(FVector(700,100,100),FRotator::ZeroRotator);
    if (!Turret) return false;
    Turret->DispatchBeginPlay(); Turret->SetActorTickEnabled(false);
    Turret->InitializeDeployable(EBreakerDeployableType::Turret,Far,0);
    Damage(Far,Turret,20);
    TestTrue(TEXT("Direct deployable producer owns threat"),Select()==Turret);
    TestTrue(TEXT("Owner reward attribution remains unchanged"),Observer->LastRewardOwner==Far);
    TestTrue(TEXT("Hit carries separate direct threat producer"),Observer->LastThreatSource==Turret);
    auto* TurretCombat=Turret->FindComponentByClass<UBreakerCombatComponent>();
    auto* TurretHealth=Cast<UBreakerAttributeSet>(Turret->GetDefaultSubobjectByName(TEXT("Attributes")));
    if (!TurretCombat || !TurretHealth) return false;
    Turret->SetActorLocation(FVector(100,0,100));
    const float BeforeStrike=TurretHealth->GetHealth();
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT { GFrameCounter=Frame; };
    ++GFrameCounter; World->Tick(LEVELTICK_All,2.0f);
    Select();
    TestTrue(TEXT("Actual native melee damages its selected deployable"),TurretHealth->GetHealth()<BeforeStrike);
    TestTrue(TEXT("Committed attack exposes actual deployable target"),Enemy->GetCommittedAttackTarget()==Turret);
    Turret->Destroy();
    // Neither tied player is the removed current target; distance now breaks
    // their equal scores. Retention does not resurrect a historical target.
    TestTrue(TEXT("Destroyed target is pruned and tied survivors use distance"),Select()==Near);
    Damage(Far,nullptr,1);
    TestTrue(TEXT("Additional earned threat selects the player about to die"),Select()==Far);
    FBreakerDamageRequest Kill; Kill.BaseDamage=Far->GetAttributes()->GetHealth()+Far->GetAttributes()->GetShield()+1;
    Kill.DamageFamily=EBreakerDamageFamily::TrueDamage; Kill.bCanCritical=false; Kill.bCanBeAvoided=false;
    Far->GetCombat()->ReceiveDamage(Kill);
    TestTrue(TEXT("Dead player no longer eligible"),Select()==Near);
    Enemy->ConfigureEncounter(Enemy->GetActorLocation(),0);
    TestNull(TEXT("Encounter reset clears selected target"),Enemy->GetThreatTarget());
    TestTrue(TEXT("Reset restores ordinary zero-threat acquisition"),Select()==Near);
    // Exercise the actual authority safe-zone predicate with its authored
    // GameMode initialization, without beginning any owner Character/save path.
    auto* Session=NewObject<UBreakerGameInstance>();
    World->SetGameInstance(Session);
    GEngine->GetWorldContextFromWorldChecked(World).OwningGameInstance=Session;
    World->GetWorldSettings()->DefaultGameMode=ABreakerGameMode::StaticClass();
    if (!TestTrue(TEXT("Native authority mode installed"),World->SetGameMode(FURL()))) return false;
    auto* Mode=World->GetAuthGameMode<ABreakerGameMode>();
    if (!Mode) return false;
    Mode->DispatchBeginPlay();
    auto* Controller=World->SpawnActor<APlayerController>();
    if (!Controller) return false;
    Controller->Possess(Near);
    // The native starting-player path establishes the field frame and safe
    // ring; GameMode BeginPlay alone deliberately does neither.
    Mode->HandleStartingNewPlayer_Implementation(Controller);
    const FVector Safe=Mode->GetSafeZoneCenter()+FVector(0,0,100);
    if (!TestTrue(TEXT("Actual safe-zone center active"),Mode->IsInSafeZone(Safe))) return false;
    Near->SetActorLocation(Safe);
    TestFalse(TEXT("Live player in actual safe zone is ineligible"),Enemy->IsEligibleThreatTarget(Near));
    Far->GetCombat()->RestoreVitals();
    Far->SetActorLocation(Safe+FVector(Mode->GetSafeZoneRadius()+300,0,0));
    TestTrue(TEXT("Live player outside actual safe zone remains eligible"),Enemy->IsEligibleThreatTarget(Far));
    return true;
}
#endif
