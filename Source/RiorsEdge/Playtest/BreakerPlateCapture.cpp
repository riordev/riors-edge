#include "Playtest/BreakerPlateCapture.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/BreakerCharacter.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Game/BreakerGameInstance.h"
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
#include "Interaction/BreakerBasinRecorder.h"
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
 FString Mode,UserDirectory,VolatileMode,AbilityClass,RecorderMode;
 const bool bRecorder=FParse::Value(FCommandLine::Get(),TEXT("BreakerCaptureRecorder="),RecorderMode);
 const bool bAbilityMenu=FParse::Value(FCommandLine::Get(),TEXT("BreakerCaptureAbilityClass="),AbilityClass);
 const bool bVolatile=FParse::Value(FCommandLine::Get(),TEXT("BreakerCaptureVolatileFreeze="),VolatileMode);
 const bool bPlate=FParse::Value(FCommandLine::Get(),TEXT("BreakerCaptureEnemyPlate="),Mode);
 const bool bCache=FParse::Param(FCommandLine::Get(),TEXT("BreakerCaptureCache"));
 const bool bScenery=FParse::Param(FCommandLine::Get(),TEXT("BreakerCaptureScenery"));
 if(!World||(!bPlate&&!bCache&&!bScenery&&!bVolatile&&!bAbilityMenu&&!bRecorder))return;
 if(bRecorder && (bPlate||bCache||bScenery||bVolatile||bAbilityMenu
    ||(!RecorderMode.Equals(TEXT("Recovery"),ESearchCase::IgnoreCase)&&!RecorderMode.Equals(TEXT("Extraction"),ESearchCase::IgnoreCase))))
 { UE_LOG(LogTemp,Warning,TEXT("[PlateCapture] Recorder requires Recovery or Extraction and no other setup mode."));return; }
 if(bAbilityMenu)
 {
    FString Board;
    if(bPlate||bCache||bScenery||bVolatile
        || (!AbilityClass.Equals(TEXT("Swift"),ESearchCase::IgnoreCase) && !AbilityClass.Equals(TEXT("Caster"),ESearchCase::IgnoreCase))
        || !FParse::Value(FCommandLine::Get(),TEXT("BreakerCaptureBoard="),Board)
        || !Board.Equals(TEXT("ABILITIES"),ESearchCase::IgnoreCase))
    { UE_LOG(LogTemp,Warning,TEXT("[PlateCapture] AbilityClass requires Swift or Caster, -BreakerCaptureBoard=ABILITIES, and no other setup flag.")); return; }
 }
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
 FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(World,[World,Mode,bCache,bPlate,bScenery,bVolatile,bAbilityMenu,AbilityClass,bRecorder,RecorderMode](float)
 {
    if(bAbilityMenu||bRecorder)
        if(const auto* Session=World->GetGameInstance<UBreakerGameInstance>(); Session && Session->IsArrivalCoverUp()) return true;
    auto* PC=World->GetFirstPlayerController();auto* Player=PC?Cast<ABreakerCharacter>(PC->GetPawn()):nullptr;
    if(!Player||!Player->HasAuthority())return false;
    Player->bRefuseSavesForPendingCharacter=true;
    if(bAbilityMenu)
    {
        auto* Progression=Player->GetProgression();
        const auto Class=AbilityClass.Equals(TEXT("Swift"),ESearchCase::IgnoreCase) ? EBreakerClassId::Swift : EBreakerClassId::Caster;
        if(!Progression) return false;
        // Disposable capture pawns auto-lock Swift before setup. Reset only this
        // isolated nonsaving fixture, then use ordinary class choice/grant rules.
        Progression->bAutoLockSwiftIfFresh=false;
        Progression->LoadProgressionState(FBreakerProgressionState{});
        if(!Progression->ChoosePermanentClassById(Class))
        { UE_LOG(LogTemp,Warning,TEXT("[PlateCapture] Ability menu refused: requires a fresh unchosen character and normal class selection.")); return false; }
        Player->GetAbilities()->RefreshGrants();
        Player->OpenMenuScreenForCapture(TEXT("INVENTORY")); // Existing ABILITIES board routing.
        UE_LOG(LogTemp,Display,TEXT("[PlateCapture] Ability menu class=%s level=%d through normal choice/registered grants; no XP or extra unlocks."),
            *AbilityClass,Progression->GetCharacterLevel());
        return false;
    }
    Player->ResumeFromMenu();
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
    if(bRecorder)
    {
        ABreakerBasinRecorder* Recovery=nullptr;ABreakerBasinRecorder* Extraction=nullptr;
        for(TActorIterator<ABreakerBasinRecorder> It(World);It;++It)
            if(It->IsExtraction())Extraction=*It;else Recovery=*It;
        if(!Recovery||!Extraction){UE_LOG(LogTemp,Warning,TEXT("[PlateCapture] Recorder setup requires actual Red Basin consoles."));return false;}
        auto StandAt=[&](ABreakerBasinRecorder* Console)
        {
            // O2 PLACEHOLDER capture distance only. Real standing capsule rests on the
            // existing floor; neither console nor its scenery is relocated.
            FVector At=Console->GetActorLocation()+Console->GetActorForwardVector()*180.f;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(RecorderCaptureFloor),false,Player);Query.AddIgnoredActor(Console);
            FHitResult Floor;
            if(!World->LineTraceSingleByObjectType(Floor,At+FVector(0,0,200),At-FVector(0,0,500),FCollisionObjectQueryParams(ECC_WorldStatic),Query))return false;
            const auto* Body=Player->GetCapsuleComponent();
            At.Z=Floor.ImpactPoint.Z+Body->GetScaledCapsuleHalfHeight()+2.f;
            if(World->OverlapAnyTestByObjectType(At,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),
                FCollisionShape::MakeCapsule(Body->GetScaledCapsuleRadius(),Body->GetScaledCapsuleHalfHeight()),Query))return false;
            Player->SetActorLocation(At);
            return Console->IsInteractionReachable(Player);
        };
        ABreakerBasinRecorder* Selected=Recovery;
        if(RecorderMode.Equals(TEXT("Extraction"),ESearchCase::IgnoreCase))
        {
            if(!StandAt(Recovery)||!Recovery->TryInteract(Player))
            {UE_LOG(LogTemp,Warning,TEXT("[PlateCapture] Recorder extraction refused: real recovery interaction did not succeed."));return false;}
            Selected=Extraction;
        }
        if(!StandAt(Selected))
        {UE_LOG(LogTemp,Warning,TEXT("[PlateCapture] Recorder standing approach/visibility is obstructed."));return false;}
        Aim=Selected->GetActorLocation();
        UE_LOG(LogTemp,Display,TEXT("[PlateCapture] Recorder=%s actualCurrentStep=%d reachable=%d nearby=%d standing=%s console=%s; native recovery used for extraction, no flag injection; combat frozen for static QA."),
            *RecorderMode,Selected->IsCurrentStep(Player),Selected->IsInteractionReachable(Player),Player->FindNearbyNPC()==Selected,
            *Player->GetActorLocation().ToString(),*Selected->GetActorLocation().ToString());
    }
    else if(bCache)
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
    if(bCache)
    {
        ABreakerFernhallCache* Cache=nullptr;
        for(TActorIterator<ABreakerFernhallCache> It(World);It;++It){Cache=*It;break;}
        if(Cache)
        {
            FVector2D Anchor; const bool bProjected=PC->ProjectWorldLocationToScreen(Cache->GetActorLocation()+FVector(0,0,150),Anchor);
            int32 Width=0,Height=0;PC->GetViewportSize(Width,Height);
            UE_LOG(LogTemp,Display,TEXT("[PlateCapture] cache focus=%d reachable=%d distance=%.2f range=%.2f projected=%d anchor=(%.1f,%.1f) viewport=%dx%d eye=%s"),
                Player->FindNearbyNPC()==Cache,Cache->IsInteractionReachable(Player),FVector::Distance(Player->GetActorLocation(),Cache->GetActorLocation()),Cache->GetInteractionRange(),
                bProjected,Anchor.X,Anchor.Y,Width,Height,*Eye.ToString());
        }
    }
    if(PlateEnemy)UE_LOG(LogTemp,Display,TEXT("[PlateCapture] mode=%s nativeVisibility=%d eye=%s head=%s"),*Mode,
        BreakerEnemyPlateVisibility::IsVisible(World,Eye,Aim,PlateEnemy,Player),*Eye.ToString(),*Aim.ToString());
    PC->SetPause(true);
    return false; // One setup only; screenshots use the existing core ticker.
 }),1.f);
}
