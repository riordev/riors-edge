#include "Playtest/BreakerPlateCapture.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerBossEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerModifierComponent.h"
#include "Combat/BreakerEnemyPlateVisibility.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Interaction/BreakerFernhallCache.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Containers/Ticker.h"
namespace
{
 AStaticMeshActor* BreakerPlateCaptureBox(UWorld* World,FVector At,FVector Extent)
 {
    auto* Actor=World->SpawnActor<AStaticMeshActor>();if(!Actor)return nullptr;
    auto* Mesh=Actor->GetStaticMeshComponent();Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Mesh->SetWorldScale3D(Extent/50.f);Actor->SetActorLocation(At);
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Mesh->SetCollisionObjectType(ECC_WorldStatic);
    Mesh->SetCollisionResponseToAllChannels(ECR_Block);return Actor;
 }
}
void BreakerSchedulePlateCapture(UWorld* World)
{
 FString Mode,UserDirectory,VolatileMode;
 const bool bVolatile=FParse::Value(FCommandLine::Get(),TEXT("BreakerCaptureVolatileFreeze="),VolatileMode);
 const bool bPlate=FParse::Value(FCommandLine::Get(),TEXT("BreakerCaptureEnemyPlate="),Mode);
 const bool bCache=FParse::Param(FCommandLine::Get(),TEXT("BreakerCaptureCache"));
 const bool bScenery=FParse::Param(FCommandLine::Get(),TEXT("BreakerCaptureScenery"));
 if(!World||(!bPlate&&!bCache&&!bScenery&&!bVolatile))return;
 if(bVolatile)
 {
    if(bPlate||bCache||bScenery || (!VolatileMode.Equals(TEXT("open"),ESearchCase::IgnoreCase)
        && !VolatileMode.Equals(TEXT("blocked"),ESearchCase::IgnoreCase)
        && !VolatileMode.Equals(TEXT("edge"),ESearchCase::IgnoreCase)))
    { UE_LOG(LogTemp,Warning,TEXT("[PlateCapture] VolatileFreeze requires one mode: open, blocked, edge; no other capture setup flag.")); return; }
    Mode=VolatileMode;
 }
 if(!FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDirectory)||UserDirectory.IsEmpty()
    ||FPaths::IsRelative(UserDirectory)||FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(UserDirectory),FPaths::ProjectDir())
    ||IFileManager::Get().DirectoryExists(*(FPaths::ProjectSavedDir()/TEXT("SaveGames"))))
 {UE_LOG(LogTemp,Warning,TEXT("[PlateCapture] Refused: fresh isolated absolute UserDir required."));return;}
 if(bPlate&&!Mode.Equals(TEXT("open"),ESearchCase::IgnoreCase)&&!Mode.Equals(TEXT("blocked"),ESearchCase::IgnoreCase))
 {UE_LOG(LogTemp,Warning,TEXT("[PlateCapture] Mode must be open or blocked."));return;}
 // Core time advances while the arrival menu has paused world timers.
 // Weak UObject binding prevents setup after this world is destroyed.
 FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(World,[World,Mode,bCache,bPlate,bScenery,bVolatile](float)
 {
    auto* PC=World->GetFirstPlayerController();auto* Player=PC?Cast<ABreakerCharacter>(PC->GetPawn()):nullptr;
    if(!Player||!Player->HasAuthority())return false;
    Player->bRefuseSavesForPendingCharacter=true;Player->ResumeFromMenu();
    if (bScenery && !bPlate && !bCache)
    {
        // The authored tour owns location and aim on its existing core ticker.
        // Freeze simulation only: no enemies, geometry, materials, or camera
        // are replaced or repositioned by this option.
        auto* Movement = Player->GetBreakerMovement();
        Movement->StopMovementImmediately();
        Movement->SetMovementMode(MOVE_None);
        Movement->SetComponentTickEnabled(false);
        PC->SetPause(true);
        UE_LOG(LogTemp, Display, TEXT("[PlateCapture] Scenery simulation paused; authored tour retains view control."));
        return false;
    }
    for(TActorIterator<ABreakerEnemy> It(World);It;++It)
    {It->SetActorTickEnabled(false);if(auto* Move=It->FindComponentByClass<UBreakerEnemyMovementComponent>()){Move->StopMovementImmediately();Move->SetComponentTickEnabled(false);}}
    Player->GetBreakerMovement()->StopMovementImmediately();Player->GetBreakerMovement()->SetMovementMode(MOVE_None);
    FVector Aim;
    ABreakerEnemy* PlateEnemy=nullptr;
    if(bCache)
    {
        ABreakerFernhallCache* Cache=nullptr;for(TActorIterator<ABreakerFernhallCache> It(World);It;++It){Cache=*It;break;}
        if(!Cache){UE_LOG(LogTemp,Warning,TEXT("[PlateCapture] No authored Fernhall cache found."));return false;}
        Player->SetActorLocation(Cache->GetActorLocation()+Cache->GetActorForwardVector()*260.f);
        Aim=Cache->GetActorLocation();
        UE_LOG(LogTemp,Display,TEXT("[PlateCapture] Actual Fernhall cache at %s; cleared=%d opened=%d"),*Cache->GetActorLocation().ToString(),Cache->IsPocketCleared(),Cache->IsOpened());
    }
    else
    {
        // O2 PLACEHOLDER: isolated elevated visibility station, unchanged game assets.
        const FVector Origin(0,0,20000);
        BreakerPlateCaptureBox(World,Origin+FVector(900,0,-25),FVector(2200,1600,25));
        Player->SetActorLocation(Origin+FVector(0,0,100));
        FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        const TSubclassOf<ABreakerEnemy> BodyClass=bVolatile ? ABreakerEnemy::StaticClass() : ABreakerBossEnemy::StaticClass();
        auto* Boss=World->SpawnActor<ABreakerEnemy>(BodyClass,Origin+FVector(1600,0,200),FRotator(0,180,0),Spawn);if(!Boss)return false;
        Boss->ConfigureCrowdProbe();Boss->SetActorTickEnabled(false);
        if(auto* Move=Boss->FindComponentByClass<UBreakerEnemyMovementComponent>()){Move->StopMovementImmediately();Move->SetComponentTickEnabled(false);}
        const auto* Capsule=Boss->FindComponentByClass<UCapsuleComponent>();
        Boss->SetActorLocation(Origin+FVector(1600,0,Capsule->GetScaledCapsuleHalfHeight()+2.f));
        Aim=Boss->GetActorLocation()+FVector(0,0,Capsule->GetScaledCapsuleHalfHeight());PlateEnemy=Boss;
        if(Mode.Equals(TEXT("blocked"),ESearchCase::IgnoreCase))BreakerPlateCaptureBox(World,Origin+FVector(800,0,500),FVector(100,500,500));
    }
    if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(.05f);
    FVector Eye;FRotator Facing;PC->GetPlayerViewPoint(Eye,Facing);PC->SetControlRotation((Aim-Eye).Rotation());
    if(PC->PlayerCameraManager)PC->PlayerCameraManager->UpdateCamera(.05f);
    PC->GetPlayerViewPoint(Eye,Facing);
    if(bVolatile && PlateEnemy)
    {
        auto* Modifiers=PlateEnemy->GetModifierComponent();
        auto* Combat=PlateEnemy->FindComponentByClass<UBreakerCombatComponent>();
        if(!Modifiers||!Combat||!Modifiers->SetModifiers({EBreakerEnemyModifier::Volatile})) return false;
        // Structural capture: normal accepted lethal request starts the real
        // corpse fuse. Its authored duration and damage are never replaced.
        FBreakerDamageRequest Hit; Hit.BaseDamage=Combat->GetMaxHealth()*2.f;
        Hit.DamageFamily=EBreakerDamageFamily::TrueDamage; Hit.bCanCritical=false;
        Hit.bCanBeAvoided=false; Hit.bBypassShield=true; Hit.SetInstigator(Player);
        const auto Result=Combat->ReceiveDamage(Hit);
        if(!Result.bKilled||!Modifiers->IsFuseLit())
        { UE_LOG(LogTemp,Warning,TEXT("[PlateCapture] Volatile freeze refused: native death did not start a positive fuse.")); return false; }
        if(Mode.Equals(TEXT("edge"),ESearchCase::IgnoreCase) && PC->PlayerCameraManager)
        {
            // O2 PLACEHOLDER capture framing: head remains just inside the
            // right edge; the wider warning must be wholly culled, not clipped.
            Facing.Yaw-=PC->PlayerCameraManager->GetFOVAngle()*.49f;
            PC->SetControlRotation(Facing);PC->PlayerCameraManager->UpdateCamera(.05f);
        }
        UE_LOG(LogTemp,Display,TEXT("[PlateCapture] Volatile STATIC FREEZE mode=%s actual-death=%d remaining=%.3f authored=%.3f; validates geometry only, not countdown animation."),
            *Mode,PlateEnemy->IsDeadEnemy(),Modifiers->GetFuseRemainingSeconds(),Modifiers->Params.VolatileFuseSeconds);
    }
    if(PlateEnemy)UE_LOG(LogTemp,Display,TEXT("[PlateCapture] mode=%s nativeVisibility=%d eye=%s head=%s"),*Mode,
        BreakerEnemyPlateVisibility::IsVisible(World,Eye,Aim,PlateEnemy,Player),*Eye.ToString(),*Aim.ToString());
    PC->SetPause(true);
    return false; // One setup only; screenshots use the existing core ticker.
 }),1.f);
}
