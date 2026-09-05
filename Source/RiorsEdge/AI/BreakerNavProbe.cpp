// The nav probe (NAV-1): the photograph the order asks for.
//
// "Done when: an enemy spawned behind a wall in the gym reaches the player
// without touching it, photographed from two vantages." The harness cannot
// play, so this builds the situation: a wall in front of the pawn, one melee
// enemy behind it, two cameras, and a log line every half second reading the
// distance, the behaviour state, the locomotion mode and the wall-touch count
// the mover keeps. REACHED is printed once, with the touch count, when the
// body is inside attack range.
//
//   bash Scripts/ue-capture.sh Gym -BreakerScreenshots=6 -ExecCmds="Breaker.Nav.Probe"
//
// Armed exactly like Combat/BreakerBarProbe: a core ticker that waits for the
// player pawn, because -ExecCmds fires on the front-end map before the
// autoplay travel lands in the gym. The probe owns the view target — the
// harness only re-targets when a capture tour exists — and flips it between
// the two vantages every screenshot interval, so frame 0 is the pawn's own
// eye (a wall, nothing behind it) and every later frame alternates A and B.
#if !UE_BUILD_SHIPPING

#include "AI/BreakerEnemyMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerEnemy.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

namespace
{
    // Geometry of the situation. The wall is wide enough that the straight
    // line is blocked from anywhere near the pawn, and short enough that the
    // path around it is a few seconds at sprint. O2 PLACEHOLDER, all of it —
    // this is an instrument, not content.
    constexpr float BreakerNavProbePawnAdvanceCm = 3200.0f;      // past the 1800 cm safe zone, onto the apron
    constexpr float BreakerNavProbeWallDistanceCm = 900.0f;
    constexpr float BreakerNavProbeWallWidthCm = 1200.0f;
    constexpr float BreakerNavProbeWallThicknessCm = 30.0f;
    constexpr float BreakerNavProbeWallHeightCm = 300.0f;
    constexpr float BreakerNavProbeEnemyDistanceCm = 1800.0f;   // inside DetectionRange (2200)
    constexpr float BreakerNavProbeReportSeconds = 0.5f;
    constexpr float BreakerNavProbeVantageSeconds = 2.0f;       // the harness's ScreenshotIntervalSeconds
    constexpr float BreakerNavProbeRetrySeconds = 0.5f;
    constexpr int32 BreakerNavProbeMaxAttempts = 120;
    // Ten half-seconds: the gym has finished building, and the harness's
    // first frame (6 s) lands a second into the approach rather than after
    // it — the first film had the whole walk before shot 0.
    constexpr int32 BreakerNavProbeMinAttempts = 10;
    // The body's visual forward against the actor's forward, in the ground
    // plane. The fit yaws the mesh onto +X, so anything past this is a mech
    // looking sideways — the gym's "everyone looking left" as a number.
    constexpr float BreakerNavProbeFacingToleranceDeg = 15.0f;  // O2 PLACEHOLDER

    float BreakerNavProbeDegreesBetween2D(const FVector& A, const FVector& B)
    {
        const FVector FlatA = A.GetSafeNormal2D();
        const FVector FlatB = B.GetSafeNormal2D();
        if (FlatA.IsNearlyZero() || FlatB.IsNearlyZero()) return 0.0f;
        return FMath::RadiansToDegrees(static_cast<float>(
            FMath::Acos(FMath::Clamp(FVector::DotProduct(FlatA, FlatB), -1.0, 1.0))));
    }

    UWorld* BreakerNavProbeCurrentWorld()
    {
        if (!GEngine) return nullptr;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
            {
                if (Context.World()) return Context.World();
            }
        }
        return nullptr;
    }

    ABreakerCharacter* BreakerNavProbeFindPlayer(UWorld* World)
    {
        for (TActorIterator<ABreakerCharacter> It(World); It; ++It)
        {
            if (*It) return *It;
        }
        return nullptr;
    }

    // The floor under a spot. The pawn is ignored explicitly: the first film
    // traced from above the pawn's own capsule, read its crown as the ground,
    // and hung the wall two and a half metres in the air — the enemy walked
    // under it and the probe reported a clean reach through nothing.
    float BreakerNavProbeGroundZ(UWorld* World, const FVector& Near, float Fallback, const AActor* Ignore)
    {
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerNavProbeGround), false, Ignore);
        if (World->LineTraceSingleByChannel(Hit, Near + FVector(0, 0, 200.0f), Near - FVector(0, 0, 4000.0f),
            ECC_WorldStatic, Params))
        {
            return Hit.ImpactPoint.Z;
        }
        return Fallback;
    }

    ACameraActor* BreakerNavProbeCamera(UWorld* World, const FVector& At, const FVector& LookAt)
    {
        FActorSpawnParameters Params;
        Params.ObjectFlags |= RF_Transient;
        ACameraActor* Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), At, (LookAt - At).Rotation(), Params);
        if (Camera && Camera->GetCameraComponent()) Camera->GetCameraComponent()->SetFieldOfView(90.0f);
        return Camera;
    }

    struct FBreakerNavProbeState
    {
        TWeakObjectPtr<ABreakerEnemy> Enemy;
        TWeakObjectPtr<ABreakerCharacter> Player;
        TWeakObjectPtr<ACameraActor> VantageA;
        TWeakObjectPtr<ACameraActor> VantageB;
        double StartTime = 0.0;
        bool bReached = false;
        bool bOnA = true;
    };

    void BreakerNavProbePlaceNow(UWorld* World, ABreakerCharacter* Player)
    {
        // OUT OF THE SAFE ZONE FIRST. The gym's spawn sits inside a 1800 cm
        // safe zone, and an enemy nulls its target before detection is even
        // consulted when the player stands in one (BreakerEnemy::Tick), so a
        // scene built at the spawn holds the enemy in PATROL forever — the
        // first run of this probe did exactly that. The pawn walks the apron
        // forward instead: far enough that the zone is behind it, on floor
        // the gym authors for every run.
        const FVector F = Player->GetActorForwardVector().GetSafeNormal2D();
        {
            const FVector Start = Player->GetActorLocation();
            const FVector Ahead = Start + F * BreakerNavProbePawnAdvanceCm;
            const float AheadGroundZ = BreakerNavProbeGroundZ(World, Ahead, Start.Z - 90.0f, Player);
            Player->TeleportTo(FVector(Ahead.X, Ahead.Y, AheadGroundZ + 100.0f), F.Rotation());
            if (APlayerController* Controller = World->GetFirstPlayerController())
            {
                Controller->SetControlRotation(F.Rotation());
            }
        }
        const FVector P = Player->GetActorLocation();
        const FVector R = FVector::CrossProduct(FVector::UpVector, F).GetSafeNormal2D();
        const float GroundZ = BreakerNavProbeGroundZ(World, P, P.Z - 90.0f, Player);
        // Each piece stands on ITS OWN floor, traced at its own spot.
        const FVector WallFoot = FVector(P.X, P.Y, GroundZ) + F * BreakerNavProbeWallDistanceCm;
        const float WallGroundZ = BreakerNavProbeGroundZ(World, WallFoot, GroundZ, Player);
        const FVector EnemyFoot = FVector(P.X, P.Y, GroundZ) + F * BreakerNavProbeEnemyDistanceCm;
        const float EnemyGroundZ = BreakerNavProbeGroundZ(World, EnemyFoot, GroundZ, Player);

        // The wall: a basic cube, world-static, collides. Same recipe as the
        // gym's own blocks, so the navmesh sees it the way it sees them.
        {
            FActorSpawnParameters Params;
            Params.ObjectFlags |= RF_Transient;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            // Sunk 50 cm so no seam shows under it on uneven ground.
            const FVector Centre = FVector(WallFoot.X, WallFoot.Y, WallGroundZ - 50.0f + BreakerNavProbeWallHeightCm * 0.5f);
            if (AStaticMeshActor* Wall = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
                Centre, F.Rotation(), Params))
            {
                UStaticMeshComponent* Mesh = Wall->GetStaticMeshComponent();
                Mesh->SetMobility(EComponentMobility::Movable);
                Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
                Mesh->SetWorldScale3D(FVector(BreakerNavProbeWallThicknessCm, BreakerNavProbeWallWidthCm,
                    BreakerNavProbeWallHeightCm) / 100.0f);
                Mesh->SetMobility(EComponentMobility::Static);
                Wall->SetActorLabel(TEXT("Runtime_NavProbeWall"));
            }
        }

        TSharedPtr<FBreakerNavProbeState> State = MakeShared<FBreakerNavProbeState>();
        State->Player = Player;
        State->StartTime = World->GetTimeSeconds();

        // The enemy, behind the wall, facing the player. A crowd-probe body:
        // no loot, no respawn, so the frame is one body and one wall.
        {
            FActorSpawnParameters Params;
            Params.ObjectFlags |= RF_Transient;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            const FVector Spot = FVector(EnemyFoot.X, EnemyFoot.Y, EnemyGroundZ + 100.0f);
            ABreakerEnemy* Enemy = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), Spot, (-F).Rotation(), Params);
            if (!Enemy)
            {
                UE_LOG(LogTemp, Warning, TEXT("[BreakerNavProbe] enemy spawn failed; nothing to photograph."));
                return;
            }
            Enemy->ConfigureCrowdProbe();
            Enemy->SetAreaLevel(10);
            Enemy->ConfigureEncounter(Spot, 0.0f);
            State->Enemy = Enemy;
        }

        // Vantage A: high and to the right, on the pawn's side, looking at
        // the wall's centre — the whole detour in one frame. Vantage B: low,
        // beyond the wall and to the left, looking back at the pawn — the
        // enemy's approach as the player will never see it.
        const FVector WallCentre = FVector(WallFoot.X, WallFoot.Y, WallGroundZ + 150.0f);
        State->VantageA = BreakerNavProbeCamera(World,
            FVector(P.X, P.Y, GroundZ) - F * 600.0f + R * 1100.0f + FVector(0, 0, 1000.0f), WallCentre);
        State->VantageB = BreakerNavProbeCamera(World,
            FVector(P.X, P.Y, GroundZ) + F * 2700.0f - R * 1000.0f + FVector(0, 0, 260.0f), P);

        APlayerController* PC = World->GetFirstPlayerController();
        if (PC && State->VantageA.IsValid()) PC->SetViewTarget(State->VantageA.Get());

        UE_LOG(LogTemp, Display,
            TEXT("[BreakerNavProbe] wall %.0f cm ahead (%.0f wide, %.0f tall, floor z %.0f), enemy %.0f cm ahead (floor z %.0f), pawn at (%.0f, %.0f, %.0f) on floor z %.0f."),
            BreakerNavProbeWallDistanceCm, BreakerNavProbeWallWidthCm, BreakerNavProbeWallHeightCm, WallGroundZ,
            BreakerNavProbeEnemyDistanceCm, EnemyGroundZ, P.X, P.Y, P.Z, GroundZ);

        // The vantage flip, on the harness's cadence.
        FTimerHandle VantageTimer;
        World->GetTimerManager().SetTimer(VantageTimer, FTimerDelegate::CreateLambda([State, World]()
        {
            APlayerController* Controller = World->GetFirstPlayerController();
            if (!Controller) return;
            State->bOnA = !State->bOnA;
            ACameraActor* Next = State->bOnA ? State->VantageA.Get() : State->VantageB.Get();
            if (Next) Controller->SetViewTarget(Next);
        }), BreakerNavProbeVantageSeconds, true);

        // The readout.
        FTimerHandle ReportTimer;
        World->GetTimerManager().SetTimer(ReportTimer, FTimerDelegate::CreateLambda([State, World]()
        {
            ABreakerEnemy* Enemy = State->Enemy.Get();
            ABreakerCharacter* Target = State->Player.Get();
            if (!Enemy || !Target) return;
            const float Elapsed = static_cast<float>(World->GetTimeSeconds() - State->StartTime);
            const float Distance = FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation());
            const UBreakerEnemyMovementComponent* Mover = Enemy->GetEnemyMovement();
            const int32 Touches = Mover ? Mover->GetWorldTouchCount() : -1;
            const TCHAR* Mode = !Mover ? TEXT("none")
                : Mover->GetLastMode() == EBreakerLocomotionMode::Path ? TEXT("PATH")
                : Mover->GetLastMode() == EBreakerLocomotionMode::Steer ? TEXT("STEER") : TEXT("IDLE");
            UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
            const TCHAR* Nav = !NavSys ? TEXT("none")
                : !NavSys->GetDefaultNavDataInstance() ? TEXT("MISSING")
                : NavSys->IsNavigationBuildInProgress() ? TEXT("building") : TEXT("built");
            // facing: the body's visual forward against the actor's forward —
            // the invariant the fit holds in every state, detour or not.
            // toplayer: that same visual forward against the line to the
            // player, which legitimately opens up mid-detour, so it is read
            // and not judged.
            const FVector BodyForward = Enemy->GetNamedBodyWorldForward();
            const float Facing = BreakerNavProbeDegreesBetween2D(BodyForward, Enemy->GetActorForwardVector());
            const float ToPlayer = BreakerNavProbeDegreesBetween2D(BodyForward,
                Target->GetActorLocation() - Enemy->GetActorLocation());
            const FString StateLabel = Enemy->GetEnemyStateLabel();
            UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] t=%.1f dist=%.0f state=%s mode=%s touches=%d nav=%s facing=%.0f toplayer=%.0f"),
                Elapsed, Distance, *StateLabel, Mode, Touches, Nav, Facing, ToPlayer);
            if (Facing > BreakerNavProbeFacingToleranceDeg && StateLabel != TEXT("PATROL"))
            {
                UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] FACING FAIL body forward is %.0f deg off the actor's forward (tolerance %.0f)"),
                    Facing, BreakerNavProbeFacingToleranceDeg);
            }
            if (!State->bReached && Distance <= Enemy->GetAttackRange() + 45.0f)
            {
                State->bReached = true;
                UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] REACHED after %.1f s, touches=%d"), Elapsed, Touches);
            }
        }), BreakerNavProbeReportSeconds, true);
    }

    void BreakerNavProbeArm()
    {
        TSharedPtr<int32> Attempt = MakeShared<int32>(0);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
            [Attempt](float) -> bool
            {
                const int32 Now = (*Attempt)++;
                if (Now >= BreakerNavProbeMaxAttempts)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[BreakerNavProbe] no player pawn after %d attempts; nothing placed."), Now);
                    return false;
                }
                if (Now < BreakerNavProbeMinAttempts) return true;
                UWorld* World = BreakerNavProbeCurrentWorld();
                ABreakerCharacter* Player = World ? BreakerNavProbeFindPlayer(World) : nullptr;
                if (!World || !Player) return true;
                BreakerNavProbePlaceNow(World, Player);
                return false;
            }), BreakerNavProbeRetrySeconds);
    }

    FAutoConsoleCommandWithWorld GBreakerNavProbeCommand(
        TEXT("Breaker.Nav.Probe"),
        TEXT("Builds a wall 9 m ahead of the pawn and one melee enemy 9 m behind it, with two vantages, ")
        TEXT("and logs distance, mode and wall touches until the body reaches the pawn."),
        FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld*) { BreakerNavProbeArm(); }));
}

#endif
