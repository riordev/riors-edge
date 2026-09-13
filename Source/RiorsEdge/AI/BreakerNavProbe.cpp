// The nav probe (NAV-1, NAV-2): the photograph the order asks for.
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
// The Cover variant builds the ranged question instead: a Lattice in the open
// with a clear line to the pawn, then, two seconds in, a wall dropped on that
// line and two cover blocks registered either side of it. The readout adds the
// enemy's line-of-sight read and the firing flank it chose; RE-ACQUIRED is
// printed once when the line is open again, LOS FAIL once if it is not within
// BreakerNavProbeReacquireSeconds.
//
//   bash Scripts/ue-capture.sh Gym -BreakerScreenshots=6 -ExecCmds="Breaker.Nav.Probe Cover"
//
// The Squad variant builds the pack question (NAV-3): no wall; two closers on
// one bearing, a Lattice and a Warden, each seeded apart. The readout prints
// every body's bearing from the pawn, distance and state, then judges three
// things: the closers' bearing split once both have arrived, the Lattice's
// distance against its band once it holds, and the Warden's body forward
// against the line to the pawn.
//
//   bash Scripts/ue-capture.sh Gym -BreakerScreenshots=6 -ExecCmds="Breaker.Nav.Probe Squad"
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
#include "Combat/BreakerRangedEnemy.h"
#include "Combat/BreakerWardenEnemy.h"
#include "AI/BreakerLocomotionMath.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/BreakerGameMode.h"
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
    // Slack over the enemy's own turn cap before a sample reads as a spin: the
    // probe samples every half second, so a cap applied per tick can land a
    // few degrees over the cap across one sample without being wrong.
    constexpr float BreakerNavProbeTurnSlackDegPerSecond = 10.0f;  // O2 PLACEHOLDER

    // The Cover variant. The Lattice starts inside its hold band (900-1900)
    // with the line open; the wall lands on that line two seconds in, and the
    // two blocks stand far enough to the sides that a flank 260 cm beside
    // either one sees past the wall's 600 cm half-width and still sits inside
    // the band. All O2 PLACEHOLDER.
    constexpr float BreakerNavProbeCoverEnemyDistanceCm = 1500.0f;
    constexpr float BreakerNavProbeCoverDelaySeconds = 2.0f;
    constexpr float BreakerNavProbeFlankForwardCm = 1200.0f;
    constexpr float BreakerNavProbeFlankLateralCm = 1300.0f;
    constexpr float BreakerNavProbeFlankBlockCm = 200.0f;
    constexpr float BreakerNavProbeFlankBlockHeightCm = 120.0f;
    // How long the Lattice gets to see the pawn again after the wall lands
    // before the probe calls it a failure.
    constexpr float BreakerNavProbeReacquireSeconds = 8.0f;   // O2 PLACEHOLDER

    // The Squad variant. Two closers stand on ONE bearing, one behind the
    // other, so any split at the ring is the arrival angle's and not the
    // placement's; their seeds are the game mode's own 1.3 step apart so the
    // sign rule reads them opposite. The Lattice starts inside its hold band
    // and the Warden closer still, each off the closers' line so neither
    // stands in their lane. All O2 PLACEHOLDER.
    constexpr float BreakerNavProbeSquadCloserDistanceCm = 1800.0f;
    constexpr float BreakerNavProbeSquadCloserSpacingCm = 160.0f;
    constexpr float BreakerNavProbeSquadCloserPhaseA = 0.0f;
    constexpr float BreakerNavProbeSquadCloserPhaseB = 1.3f;
    constexpr float BreakerNavProbeSquadLatticeDistanceCm = 1500.0f;
    constexpr float BreakerNavProbeSquadLatticeBearingDeg = 20.0f;
    constexpr float BreakerNavProbeSquadLatticePhase = 0.4f;
    constexpr float BreakerNavProbeSquadWardenDistanceCm = 1200.0f;
    constexpr float BreakerNavProbeSquadWardenBearingDeg = -35.0f;
    constexpr float BreakerNavProbeSquadWardenPhase = 1.4f;
    // Slack under the authored split before the closers read as stacked: the
    // ring is 260 cm and the weave still moves the bodies once they hold, so
    // a sample can land a few degrees inside 2 x ArrivalOffsetDeg honestly.
    constexpr float BreakerNavProbeSquadSplitSlackDeg = 15.0f;   // O2 PLACEHOLDER
    // Slack on the Warden's front: its facing turns at the cap, so a sample
    // half a second after the pawn moves can legitimately be this far off.
    constexpr float BreakerNavProbeSquadFrontSlackDeg = 20.0f;   // O2 PLACEHOLDER
    // The Warden spawns facing its own line and needs the first turn to land
    // before its front is judged.
    constexpr float BreakerNavProbeSquadSettleSeconds = 1.0f;   // O2 PLACEHOLDER

    float BreakerNavProbeDegreesBetween2D(const FVector& A, const FVector& B)
    {
        const FVector FlatA = A.GetSafeNormal2D();
        const FVector FlatB = B.GetSafeNormal2D();
        if (FlatA.IsNearlyZero() || FlatB.IsNearlyZero()) return 0.0f;
        return FMath::RadiansToDegrees(static_cast<float>(
            FMath::Acos(FMath::Clamp(FVector::DotProduct(FlatA, FlatB), -1.0, 1.0))));
    }

    // A signed bearing in the ground plane: degrees from Forward towards
    // Right, so a body to the pawn's right reads positive.
    float BreakerNavProbeBearingDeg(const FVector& Forward, const FVector& Right, const FVector& To)
    {
        const FVector Flat = To.GetSafeNormal2D();
        if (Flat.IsNearlyZero()) return 0.0f;
        return FMath::RadiansToDegrees(static_cast<float>(
            FMath::Atan2(FVector::DotProduct(Flat, Right), FVector::DotProduct(Flat, Forward))));
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

    // OUT OF THE SAFE ZONE FIRST. The gym's spawn sits inside a 1800 cm
    // safe zone, and an enemy nulls its target before detection is even
    // consulted when the player stands in one (BreakerEnemy::Tick), so a
    // scene built at the spawn holds the enemy in PATROL forever — the
    // first run of this probe did exactly that. The pawn walks the apron
    // forward instead: far enough that the zone is behind it, on floor
    // the gym authors for every run.
    void BreakerNavProbeAdvancePawn(UWorld* World, ABreakerCharacter* Player)
    {
        const FVector F = Player->GetActorForwardVector().GetSafeNormal2D();
        const FVector Start = Player->GetActorLocation();
        const FVector Ahead = Start + F * BreakerNavProbePawnAdvanceCm;
        const float AheadGroundZ = BreakerNavProbeGroundZ(World, Ahead, Start.Z - 90.0f, Player);
        Player->TeleportTo(FVector(Ahead.X, Ahead.Y, AheadGroundZ + 100.0f), F.Rotation());
        if (APlayerController* Controller = World->GetFirstPlayerController())
        {
            Controller->SetControlRotation(F.Rotation());
        }
    }

    // A basic cube, world-static, collides. Same recipe as the gym's own
    // blocks, so the navmesh sees it the way it sees them. Extent is the full
    // size in cm on each axis, before the yaw.
    AStaticMeshActor* BreakerNavProbeSpawnBlock(UWorld* World, const FVector& Centre, const FVector& ExtentCm,
        const FRotator& Rotation, const TCHAR* Label)
    {
        FActorSpawnParameters Params;
        Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AStaticMeshActor* Block = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
            Centre, Rotation, Params);
        if (!Block) return nullptr;
        UStaticMeshComponent* Mesh = Block->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Mesh->SetWorldScale3D(ExtentCm / 100.0f);
        Mesh->SetMobility(EComponentMobility::Static);
        Block->SetActorLabel(Label);
        return Block;
    }

    // The wall: sunk 50 cm so no seam shows under it on uneven ground, its
    // thin axis along F so it stands across the line.
    void BreakerNavProbeSpawnWall(UWorld* World, const FVector& WallFoot, float WallGroundZ, const FVector& F)
    {
        const FVector Centre = FVector(WallFoot.X, WallFoot.Y, WallGroundZ - 50.0f + BreakerNavProbeWallHeightCm * 0.5f);
        BreakerNavProbeSpawnBlock(World, Centre,
            FVector(BreakerNavProbeWallThicknessCm, BreakerNavProbeWallWidthCm, BreakerNavProbeWallHeightCm),
            F.Rotation(), TEXT("Runtime_NavProbeWall"));
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
        // The actor yaw at the previous report, so the next one can print
        // the turn rate across the sample interval.
        float PreviousYaw = 0.0f;
        bool bHasPreviousYaw = false;
        FVector PreviousLocation = FVector::ZeroVector;
        bool bHasPreviousLocation = false;

        // The Cover variant's bookkeeping. Ranged is the same body as Enemy,
        // typed so the readout can ask it about its line and its flank.
        bool bCover = false;
        TWeakObjectPtr<ABreakerRangedEnemy> Ranged;
        double CoverPlacedTime = 0.0;
        bool bCoverPlaced = false;
        bool bLostLine = false;
        bool bReacquired = false;
        bool bLosFailPrinted = false;

        // The Squad variant's bodies. Enemy is unused; each is read by role.
        bool bSquad = false;
        TWeakObjectPtr<ABreakerEnemy> CloserA;
        TWeakObjectPtr<ABreakerEnemy> CloserB;
        TWeakObjectPtr<ABreakerRangedEnemy> Lattice;
        TWeakObjectPtr<ABreakerWardenEnemy> Warden;
        bool bClosersArrivedPrinted = false;
    };

    // One crowd-probe body: no loot, no respawn, area level 10, seeded with
    // the phase the variant asks for, spawned facing the pawn.
    template <typename TEnemy>
    TEnemy* BreakerNavProbeSpawnBody(UWorld* World, const FVector& Spot, const FVector& FaceTowards, float Phase)
    {
        FActorSpawnParameters Params;
        Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        const FVector Face = (FaceTowards - Spot).GetSafeNormal2D();
        TEnemy* Enemy = World->SpawnActor<TEnemy>(TEnemy::StaticClass(), Spot, Face.Rotation(), Params);
        if (!Enemy) return nullptr;
        Enemy->ConfigureCrowdProbe();
        Enemy->SetAreaLevel(10);
        Enemy->ConfigureEncounter(Spot, Phase);
        return Enemy;
    }

    void BreakerNavProbeStartVantageFlip(UWorld* World, const TSharedPtr<FBreakerNavProbeState>& State)
    {
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
    }

    void BreakerNavProbeStartReport(UWorld* World, const TSharedPtr<FBreakerNavProbeState>& State)
    {
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
            const FVector Location = Enemy->GetActorLocation();
            const double Displacement = State->bHasPreviousLocation
                ? FVector::Dist2D(Location, State->PreviousLocation) : 0.0;
            State->PreviousLocation = Location;
            State->bHasPreviousLocation = true;
            UE_LOG(LogTemp, Display, TEXT("[BreakerNavMotion] t=%.1f displacement=%.1f speed=%.1f held=%d pathheading=%s z=%.1f"),
                Elapsed, Displacement, Enemy->GetVelocity().Size2D(), Mover && Mover->IsBlockedHold(),
                Mover ? *Mover->GetPathHeading().ToCompactString() : TEXT("none"), Location.Z);
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
            // slide: that same visual forward against the way the body is
            // actually moving. A rig with one forward Walk cycle and no
            // strafe reads as sliding past this — the owner's "walking
            // offset" — so it is judged, outside the states that legitimately
            // stand (PATROL's arrival hold, HELD), attack in place, or back
            // off facing the player. Zero while the body stands.
            const float Slide = Mover && Mover->Velocity.SizeSquared2D() > 0.0f
                ? BreakerNavProbeDegreesBetween2D(BodyForward, Mover->Velocity)
                : 0.0f;
            const FString StateLabel = Enemy->GetEnemyStateLabel();
            // turn: the actor's yaw delta across the sample interval, in
            // deg/s. The first sample has nothing to differ against and
            // prints zero.
            const float Yaw = static_cast<float>(Enemy->GetActorRotation().Yaw);
            const float Turn = State->bHasPreviousYaw
                ? FMath::Abs(FMath::FindDeltaAngleDegrees(State->PreviousYaw, Yaw)) / BreakerNavProbeReportSeconds
                : 0.0f;
            State->PreviousYaw = Yaw;
            State->bHasPreviousYaw = true;
            if (!State->bCover)
            {
                UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] t=%.1f dist=%.0f state=%s mode=%s touches=%d nav=%s facing=%.0f toplayer=%.0f slide=%.0f turn=%.0f"),
                    Elapsed, Distance, *StateLabel, Mode, Touches, Nav, Facing, ToPlayer, Slide, Turn);
            }
            else
            {
                // los: the Lattice's own read of the line. cover: the flank it
                // is walking to, or none while the line is open or no flank
                // is in reach.
                ABreakerRangedEnemy* Ranged = State->Ranged.Get();
                const bool bLos = Ranged && Ranged->HasLineOfSightToTarget();
                FVector Goal = FVector::ZeroVector;
                const bool bHasGoal = Ranged && Ranged->GetCoverGoal(Goal);
                const FString CoverText = bHasGoal
                    ? FString::Printf(TEXT("(%.0f,%.0f)"), Goal.X, Goal.Y) : FString(TEXT("none"));
                UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] t=%.1f dist=%.0f state=%s mode=%s touches=%d nav=%s facing=%.0f toplayer=%.0f slide=%.0f turn=%.0f los=%d cover=%s"),
                    Elapsed, Distance, *StateLabel, Mode, Touches, Nav, Facing, ToPlayer, Slide, Turn, bLos ? 1 : 0, *CoverText);
                if (State->bCoverPlaced && !State->bReacquired)
                {
                    const float SinceCover = static_cast<float>(World->GetTimeSeconds() - State->CoverPlacedTime);
                    if (!bLos) State->bLostLine = true;
                    if (State->bLostLine && bLos)
                    {
                        State->bReacquired = true;
                        UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] RE-ACQUIRED after %.1f s, touches=%d"), SinceCover, Touches);
                    }
                    else if (SinceCover > BreakerNavProbeReacquireSeconds && !State->bLosFailPrinted)
                    {
                        State->bLosFailPrinted = true;
                        UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] LOS FAIL no line to the pawn %.1f s after the wall (limit %.0f), lost=%d cover=%s"),
                            SinceCover, BreakerNavProbeReacquireSeconds, State->bLostLine ? 1 : 0, *CoverText);
                    }
                }
            }
            if (Facing > BreakerNavProbeFacingToleranceDeg && StateLabel != TEXT("PATROL") && StateLabel != TEXT("EMERGING"))
            {
                UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] FACING FAIL body forward is %.0f deg off the actor's forward (tolerance %.0f)"),
                    Facing, BreakerNavProbeFacingToleranceDeg);
            }
            // A body that faces the player by rule while its feet go elsewhere
            // is not a slide: the melee's BACK OFF, and the ranged class in every
            // band (BreakerRangedEnemy pins DesiredFacing to the player, so its
            // FALLING BACK is a reverse walk and its HOLDING / AIMING strafe is
            // the muzzle held on the target). Only a body that is supposed to
            // walk where it looks can fail this.
            if (Slide > BreakerNavProbeFacingToleranceDeg && StateLabel != TEXT("PATROL") && StateLabel != TEXT("EMERGING") && StateLabel != TEXT("HELD")
                && StateLabel != TEXT("ATTACK") && StateLabel != TEXT("BACK OFF")
                && StateLabel != TEXT("FALLING BACK") && StateLabel != TEXT("HOLDING") && StateLabel != TEXT("AIMING"))
            {
                UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] SLIDE FAIL body forward is %.0f deg off its velocity (tolerance %.0f)"),
                    Slide, BreakerNavProbeFacingToleranceDeg);
            }
            if (Turn > Enemy->MaxTurnRateDegreesPerSecond + BreakerNavProbeTurnSlackDegPerSecond)
            {
                UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] TURN FAIL actor turned %.0f deg/s over the sample (cap %.0f)"),
                    Turn, Enemy->MaxTurnRateDegreesPerSecond);
            }
            // REACHED is the melee question. The Lattice has no contact range
            // to arrive inside; its answer is RE-ACQUIRED above.
            if (!State->bCover && !State->bReached && Distance <= Enemy->GetAttackRange() + 45.0f)
            {
                State->bReached = true;
                UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] REACHED after %.1f s, touches=%d"), Elapsed, Touches);
            }
        }), BreakerNavProbeReportSeconds, true);
    }

    void BreakerNavProbePlaceNow(UWorld* World, ABreakerCharacter* Player, bool bLedge = false)
    {
        if (bLedge)
        {
            // Isolate this route measurement: ordinary Fernhall population
            // killed/respawned the stationary target before the probe could
            // measure arrival. Park it without awarding kills or clearing waves.
            for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
            {
                It->SetActorTickEnabled(false);
                It->SetActorHiddenInGame(true);
                It->SetActorEnableCollision(false);
                if (auto* Movement = It->GetEnemyMovement())
                {
                    Movement->StopMovementImmediately();
                    Movement->SetComponentTickEnabled(false);
                }
            }
        }
        BreakerNavProbeAdvancePawn(World, Player);
        const FVector F = Player->GetActorForwardVector().GetSafeNormal2D();
        const FVector P = Player->GetActorLocation();
        const FVector R = FVector::CrossProduct(FVector::UpVector, F).GetSafeNormal2D();
        const float GroundZ = BreakerNavProbeGroundZ(World, P, P.Z - 90.0f, Player);
        // Each piece stands on ITS OWN floor, traced at its own spot.
        const FVector WallFoot = FVector(P.X, P.Y, GroundZ) + F * BreakerNavProbeWallDistanceCm;
        const float WallGroundZ = BreakerNavProbeGroundZ(World, WallFoot, GroundZ, Player);
        const FVector EnemyFoot = FVector(P.X, P.Y, GroundZ) + F * BreakerNavProbeEnemyDistanceCm;
        const float EnemyGroundZ = BreakerNavProbeGroundZ(World, EnemyFoot, GroundZ, Player);

        if (bLedge)
        {
            constexpr float LedgeHeightCm = 60.0f; // O2 PLACEHOLDER: below centre-ray height
            BreakerNavProbeSpawnBlock(World, FVector(WallFoot.X, WallFoot.Y, WallGroundZ + LedgeHeightCm * .5f),
                FVector(BreakerNavProbeWallThicknessCm, BreakerNavProbeWallWidthCm, LedgeHeightCm),
                F.Rotation(), TEXT("Runtime_NavProbeLedge"));
            UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] LEDGE height=60cm; body clearance must route around it."));
        }
        else BreakerNavProbeSpawnWall(World, WallFoot, WallGroundZ, F);

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

        BreakerNavProbeStartVantageFlip(World, State);
        BreakerNavProbeStartReport(World, State);
    }

    // The Cover variant (NAV-2). BreakerNavProbePlaceSquad below has the
    // same shape: advance the pawn, place, arm the
    // vantage flip and the readout.
    void BreakerNavProbePlaceCover(UWorld* World, ABreakerCharacter* Player)
    {
        BreakerNavProbeAdvancePawn(World, Player);
        const FVector F = Player->GetActorForwardVector().GetSafeNormal2D();
        const FVector P = Player->GetActorLocation();
        const FVector R = FVector::CrossProduct(FVector::UpVector, F).GetSafeNormal2D();
        const float GroundZ = BreakerNavProbeGroundZ(World, P, P.Z - 90.0f, Player);
        const FVector EnemyFoot = FVector(P.X, P.Y, GroundZ) + F * BreakerNavProbeCoverEnemyDistanceCm;
        const float EnemyGroundZ = BreakerNavProbeGroundZ(World, EnemyFoot, GroundZ, Player);

        TSharedPtr<FBreakerNavProbeState> State = MakeShared<FBreakerNavProbeState>();
        State->Player = Player;
        State->StartTime = World->GetTimeSeconds();
        State->bCover = true;

        // The Lattice, in the open, inside its hold band, facing the pawn.
        {
            FActorSpawnParameters Params;
            Params.ObjectFlags |= RF_Transient;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            const FVector Spot = FVector(EnemyFoot.X, EnemyFoot.Y, EnemyGroundZ + 100.0f);
            ABreakerRangedEnemy* Enemy = World->SpawnActor<ABreakerRangedEnemy>(ABreakerRangedEnemy::StaticClass(),
                Spot, (-F).Rotation(), Params);
            if (!Enemy)
            {
                UE_LOG(LogTemp, Warning, TEXT("[BreakerNavProbe] ranged enemy spawn failed; nothing to photograph."));
                return;
            }
            Enemy->ConfigureCrowdProbe();
            Enemy->SetAreaLevel(10);
            Enemy->ConfigureEncounter(Spot, 0.0f);
            State->Enemy = Enemy;
            State->Ranged = Enemy;
        }

        // The same two vantages as the melee probe, aimed at where the wall
        // will stand.
        const FVector WallFoot = FVector(P.X, P.Y, GroundZ) + F * BreakerNavProbeWallDistanceCm;
        const FVector WallCentre = FVector(WallFoot.X, WallFoot.Y, GroundZ + 150.0f);
        State->VantageA = BreakerNavProbeCamera(World,
            FVector(P.X, P.Y, GroundZ) - F * 600.0f + R * 1100.0f + FVector(0, 0, 1000.0f), WallCentre);
        State->VantageB = BreakerNavProbeCamera(World,
            FVector(P.X, P.Y, GroundZ) + F * 2700.0f - R * 1000.0f + FVector(0, 0, 260.0f), P);

        APlayerController* PC = World->GetFirstPlayerController();
        if (PC && State->VantageA.IsValid()) PC->SetViewTarget(State->VantageA.Get());

        UE_LOG(LogTemp, Display,
            TEXT("[BreakerNavProbe] Cover: Lattice %.0f cm ahead (floor z %.0f) with an open line; wall and two flank blocks land in %.1f s. pawn at (%.0f, %.0f, %.0f) on floor z %.0f."),
            BreakerNavProbeCoverEnemyDistanceCm, EnemyGroundZ, BreakerNavProbeCoverDelaySeconds, P.X, P.Y, P.Z, GroundZ);

        // The wall and the blocks, on the line as it stands when they land:
        // the Lattice strafes while it holds, so the line is read then, not
        // at spawn. The blocks are registered through the game mode, which is
        // the same door the field's own cover comes through.
        FTimerHandle CoverTimer;
        World->GetTimerManager().SetTimer(CoverTimer, FTimerDelegate::CreateLambda([State, World]()
        {
            ABreakerEnemy* Enemy = State->Enemy.Get();
            ABreakerCharacter* Target = State->Player.Get();
            if (!Enemy || !Target) return;
            const FVector Pawn = Target->GetActorLocation();
            const FVector Line = (Enemy->GetActorLocation() - Pawn).GetSafeNormal2D();
            const FVector Side = FVector::CrossProduct(FVector::UpVector, Line).GetSafeNormal2D();
            const float PawnGroundZ = BreakerNavProbeGroundZ(World, Pawn, Pawn.Z - 90.0f, Target);

            const FVector WallFootNow = FVector(Pawn.X, Pawn.Y, PawnGroundZ) + Line * BreakerNavProbeWallDistanceCm;
            const float WallGroundZ = BreakerNavProbeGroundZ(World, WallFootNow, PawnGroundZ, Target);
            BreakerNavProbeSpawnWall(World, WallFootNow, WallGroundZ, Line);

            ABreakerGameMode* GameMode = World->GetAuthGameMode<ABreakerGameMode>();
            for (const float Sign : { 1.0f, -1.0f })
            {
                const FVector Foot = FVector(Pawn.X, Pawn.Y, PawnGroundZ)
                    + Line * BreakerNavProbeFlankForwardCm + Side * (Sign * BreakerNavProbeFlankLateralCm);
                const float FootZ = BreakerNavProbeGroundZ(World, Foot, PawnGroundZ, Target);
                const FVector Centre = FVector(Foot.X, Foot.Y, FootZ - 5.0f + BreakerNavProbeFlankBlockHeightCm * 0.5f);
                BreakerNavProbeSpawnBlock(World, Centre,
                    FVector(BreakerNavProbeFlankBlockCm, BreakerNavProbeFlankBlockCm, BreakerNavProbeFlankBlockHeightCm),
                    Line.Rotation(), TEXT("Runtime_NavProbeFlank"));
                if (GameMode) GameMode->RegisterCoverAnchor(Centre, EBreakerCoverClass::ChestHigh, BreakerNavProbeFlankBlockHeightCm);
            }
            if (!GameMode)
            {
                UE_LOG(LogTemp, Warning, TEXT("[BreakerNavProbe] no ABreakerGameMode; the flank blocks stand unregistered and no flank can be chosen."));
            }
            State->bCoverPlaced = true;
            State->CoverPlacedTime = World->GetTimeSeconds();
            UE_LOG(LogTemp, Display,
                TEXT("[BreakerNavProbe] wall on the line %.0f cm from the pawn, flank blocks %.0f cm ahead and +/-%.0f cm beside it, registered=%d."),
                BreakerNavProbeWallDistanceCm, BreakerNavProbeFlankForwardCm, BreakerNavProbeFlankLateralCm, GameMode ? 2 : 0);
        }), BreakerNavProbeCoverDelaySeconds, false);

        BreakerNavProbeStartVantageFlip(World, State);
        BreakerNavProbeStartReport(World, State);
    }

    // The Squad readout: one line per body, then the three judgements.
    void BreakerNavProbeStartSquadReport(UWorld* World, const TSharedPtr<FBreakerNavProbeState>& State)
    {
        FTimerHandle ReportTimer;
        World->GetTimerManager().SetTimer(ReportTimer, FTimerDelegate::CreateLambda([State, World]()
        {
            ABreakerCharacter* Target = State->Player.Get();
            if (!Target) return;
            const float Elapsed = static_cast<float>(World->GetTimeSeconds() - State->StartTime);
            const FVector Pawn = Target->GetActorLocation();
            const FVector Forward = Target->GetActorForwardVector().GetSafeNormal2D();
            const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal2D();

            const auto ReadBody = [&](const TCHAR* Role, ABreakerEnemy* Enemy)
            {
                if (!Enemy) return;
                const FVector Line = Enemy->GetActorLocation() - Pawn;
                const float Bearing = BreakerNavProbeBearingDeg(Forward, Right, Line);
                const float Distance = Line.Size2D();
                const UBreakerEnemyMovementComponent* Mover = Enemy->GetEnemyMovement();
                const TCHAR* Mode = !Mover ? TEXT("none")
                    : Mover->GetLastMode() == EBreakerLocomotionMode::Path ? TEXT("PATH")
                    : Mover->GetLastMode() == EBreakerLocomotionMode::Steer ? TEXT("STEER") : TEXT("IDLE");
                const FVector BodyForward = Enemy->GetNamedBodyWorldForward();
                const float Facing = BreakerNavProbeDegreesBetween2D(BodyForward, Enemy->GetActorForwardVector());
                const float ToPlayer = BreakerNavProbeDegreesBetween2D(BodyForward, -Line);
                const FString StateLabel = Enemy->GetEnemyStateLabel();
                UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] t=%.1f %s bearing=%.0f dist=%.0f state=%s mode=%s facing=%.0f toplayer=%.0f"),
                    Elapsed, Role, Bearing, Distance, *StateLabel, Mode, Facing, ToPlayer);
                if (Facing > BreakerNavProbeFacingToleranceDeg && StateLabel != TEXT("PATROL") && StateLabel != TEXT("EMERGING"))
                {
                    UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] FACING FAIL %s body forward is %.0f deg off the actor's forward (tolerance %.0f)"),
                        Role, Facing, BreakerNavProbeFacingToleranceDeg);
                }
            };

            ABreakerEnemy* CloserA = State->CloserA.Get();
            ABreakerEnemy* CloserB = State->CloserB.Get();
            ABreakerRangedEnemy* Lattice = State->Lattice.Get();
            ABreakerWardenEnemy* Warden = State->Warden.Get();
            ReadBody(TEXT("closerA"), CloserA);
            ReadBody(TEXT("closerB"), CloserB);
            ReadBody(TEXT("lattice"), Lattice);
            ReadBody(TEXT("warden"), Warden);

            // The split: the closers' bearings from the pawn, apart. Judged
            // once both hold the ring, because on the way in they share a line
            // by construction.
            if (CloserA && CloserB)
            {
                const float Split = BreakerNavProbeDegreesBetween2D(
                    CloserA->GetActorLocation() - Pawn, CloserB->GetActorLocation() - Pawn);
                const bool bBothArrived = CloserA->GetEnemyStateLabel() == TEXT("ATTACK")
                    && CloserB->GetEnemyStateLabel() == TEXT("ATTACK");
                UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] SQUAD closers split=%.0f arrived=%d"), Split, bBothArrived ? 1 : 0);
                if (bBothArrived && !State->bClosersArrivedPrinted)
                {
                    State->bClosersArrivedPrinted = true;
                    UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] SQUAD closers ARRIVED after %.1f s, split=%.0f"), Elapsed, Split);
                }
                const float Expected = 2.0f * BreakerLocomotionMath::ArrivalOffsetDeg;
                if (bBothArrived && Split < Expected - BreakerNavProbeSquadSplitSlackDeg)
                {
                    UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] SPLIT FAIL closers %.0f deg apart at the ring (expected %.0f, slack %.0f)"),
                        Split, Expected, BreakerNavProbeSquadSplitSlackDeg);
                }
            }

            // The band-holder: inside its authored band whenever it says it
            // holds.
            if (Lattice)
            {
                const float Distance = FVector::Dist2D(Lattice->GetActorLocation(), Pawn);
                const float Min = Lattice->MinEngagementDistance;
                const float Max = Lattice->MaxEngagementDistance;
                UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] SQUAD lattice dist=%.0f band=[%.0f,%.0f] los=%d"),
                    Distance, Min, Max, Lattice->HasLineOfSightToTarget() ? 1 : 0);
                if (Lattice->GetEnemyStateLabel() == TEXT("HOLDING") && (Distance < Min || Distance > Max))
                {
                    UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] LATTICE FAIL holding at %.0f cm, outside [%.0f, %.0f]"), Distance, Min, Max);
                }
            }

            // The Warden's front: its body forward against the line to the
            // pawn, judged once the first turn has had time to land.
            if (Warden)
            {
                const float ToPlayer = BreakerNavProbeDegreesBetween2D(Warden->GetNamedBodyWorldForward(),
                    Pawn - Warden->GetActorLocation());
                UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] SQUAD warden toplayer=%.0f"), ToPlayer);
                if (Elapsed > BreakerNavProbeSquadSettleSeconds && Warden->GetEnemyStateLabel() != TEXT("PATROL") && Warden->GetEnemyStateLabel() != TEXT("EMERGING")
                    && ToPlayer > BreakerNavProbeSquadFrontSlackDeg)
                {
                    UE_LOG(LogTemp, Display, TEXT("[BreakerNavProbe] FRONT FAIL warden body forward is %.0f deg off the line to the pawn (slack %.0f, cap %.0f deg/s)"),
                        ToPlayer, BreakerNavProbeSquadFrontSlackDeg, Warden->MaxTurnRateDegreesPerSecond);
                }
            }
        }), BreakerNavProbeReportSeconds, true);
    }

    // The Squad variant (NAV-3). No wall: the question is where the bodies
    // stand relative to each other, not whether they get there.
    void BreakerNavProbePlaceSquad(UWorld* World, ABreakerCharacter* Player)
    {
        BreakerNavProbeAdvancePawn(World, Player);
        const FVector F = Player->GetActorForwardVector().GetSafeNormal2D();
        const FVector P = Player->GetActorLocation();
        const float GroundZ = BreakerNavProbeGroundZ(World, P, P.Z - 90.0f, Player);
        const FVector Origin = FVector(P.X, P.Y, GroundZ);

        TSharedPtr<FBreakerNavProbeState> State = MakeShared<FBreakerNavProbeState>();
        State->Player = Player;
        State->StartTime = World->GetTimeSeconds();
        State->bSquad = true;

        // A spot at a bearing and distance from the pawn, standing on its own
        // floor.
        const auto SpotAt = [&](float BearingDeg, float DistanceCm)
        {
            const FVector Dir = FRotator(0.0f, BearingDeg, 0.0f).RotateVector(F);
            const FVector Foot = Origin + Dir * DistanceCm;
            return FVector(Foot.X, Foot.Y, BreakerNavProbeGroundZ(World, Foot, GroundZ, Player) + 100.0f);
        };

        State->CloserA = BreakerNavProbeSpawnBody<ABreakerEnemy>(World,
            SpotAt(0.0f, BreakerNavProbeSquadCloserDistanceCm), P, BreakerNavProbeSquadCloserPhaseA);
        State->CloserB = BreakerNavProbeSpawnBody<ABreakerEnemy>(World,
            SpotAt(0.0f, BreakerNavProbeSquadCloserDistanceCm + BreakerNavProbeSquadCloserSpacingCm), P, BreakerNavProbeSquadCloserPhaseB);
        State->Lattice = BreakerNavProbeSpawnBody<ABreakerRangedEnemy>(World,
            SpotAt(BreakerNavProbeSquadLatticeBearingDeg, BreakerNavProbeSquadLatticeDistanceCm), P, BreakerNavProbeSquadLatticePhase);
        State->Warden = BreakerNavProbeSpawnBody<ABreakerWardenEnemy>(World,
            SpotAt(BreakerNavProbeSquadWardenBearingDeg, BreakerNavProbeSquadWardenDistanceCm), P, BreakerNavProbeSquadWardenPhase);
        if (!State->CloserA.IsValid() || !State->CloserB.IsValid() || !State->Lattice.IsValid() || !State->Warden.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("[BreakerNavProbe] squad spawn incomplete (closers=%d/%d lattice=%d warden=%d); the readout prints what stands."),
                State->CloserA.IsValid() ? 1 : 0, State->CloserB.IsValid() ? 1 : 0,
                State->Lattice.IsValid() ? 1 : 0, State->Warden.IsValid() ? 1 : 0);
        }

        // Vantage A: over the pawn, looking down the field — the whole
        // formation and the split in one frame. Vantage B: the pawn's own
        // eye, along its forward — the pack as the player sees it arrive.
        State->VantageA = BreakerNavProbeCamera(World,
            Origin - F * 300.0f + FVector(0, 0, 2400.0f), Origin + F * 900.0f);
        State->VantageB = BreakerNavProbeCamera(World,
            Player->GetPawnViewLocation(), Player->GetPawnViewLocation() + F * 1000.0f);

        APlayerController* PC = World->GetFirstPlayerController();
        if (PC && State->VantageA.IsValid()) PC->SetViewTarget(State->VantageA.Get());

        UE_LOG(LogTemp, Display,
            TEXT("[BreakerNavProbe] Squad: closers %.0f and %.0f cm ahead on one bearing (phase %.1f, %.1f), Lattice %.0f cm at %.0f deg (phase %.1f), Warden %.0f cm at %.0f deg (phase %.1f); offset %.0f deg. pawn at (%.0f, %.0f, %.0f) on floor z %.0f."),
            BreakerNavProbeSquadCloserDistanceCm, BreakerNavProbeSquadCloserDistanceCm + BreakerNavProbeSquadCloserSpacingCm,
            BreakerNavProbeSquadCloserPhaseA, BreakerNavProbeSquadCloserPhaseB,
            BreakerNavProbeSquadLatticeDistanceCm, BreakerNavProbeSquadLatticeBearingDeg, BreakerNavProbeSquadLatticePhase,
            BreakerNavProbeSquadWardenDistanceCm, BreakerNavProbeSquadWardenBearingDeg, BreakerNavProbeSquadWardenPhase,
            BreakerLocomotionMath::ArrivalOffsetDeg, P.X, P.Y, P.Z, GroundZ);

        BreakerNavProbeStartVantageFlip(World, State);
        BreakerNavProbeStartSquadReport(World, State);
    }

    enum class EBreakerNavProbeVariant : uint8 { Melee, Cover, Squad, Ledge };

    void BreakerNavProbeArm(EBreakerNavProbeVariant Variant)
    {
        TSharedPtr<int32> Attempt = MakeShared<int32>(0);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
            [Attempt, Variant](float) -> bool
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
                switch (Variant)
                {
                case EBreakerNavProbeVariant::Cover: BreakerNavProbePlaceCover(World, Player); break;
                case EBreakerNavProbeVariant::Squad: BreakerNavProbePlaceSquad(World, Player); break;
                case EBreakerNavProbeVariant::Ledge: BreakerNavProbePlaceNow(World, Player, true); break;
                default: BreakerNavProbePlaceNow(World, Player); break;
                }
                return false;
            }), BreakerNavProbeRetrySeconds);
    }

    FAutoConsoleCommandWithWorldAndArgs GBreakerNavProbeCommand(
        TEXT("Breaker.Nav.Probe"),
        TEXT("Builds a wall 9 m ahead of the pawn and one melee enemy 9 m behind it, with two vantages, ")
        TEXT("and logs distance, mode and wall touches until the body reaches the pawn. ")
        TEXT("'Cover': a Lattice in the open, then a wall on the line and two flank blocks; logs its line of sight and the flank it chose. ")
        TEXT("'Squad': two closers on one bearing, a Lattice and a Warden, no wall; logs each body's bearing and judges the closers' split, the Lattice's band and the Warden's front."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld*)
        {
            EBreakerNavProbeVariant Variant = EBreakerNavProbeVariant::Melee;
            if (Args.Num() > 0 && Args[0].Equals(TEXT("Cover"), ESearchCase::IgnoreCase)) Variant = EBreakerNavProbeVariant::Cover;
            if (Args.Num() > 0 && Args[0].Equals(TEXT("Squad"), ESearchCase::IgnoreCase)) Variant = EBreakerNavProbeVariant::Squad;
            if (Args.Num() > 0 && Args[0].Equals(TEXT("Ledge"), ESearchCase::IgnoreCase)) Variant = EBreakerNavProbeVariant::Ledge;
            BreakerNavProbeArm(Variant);
        }));
}

#endif
