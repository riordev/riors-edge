#include "Game/BreakerGameMode.h"

#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerTargetDummy.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerMonsterChassis.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Combat/BreakerBossEnemy.h"
#include "Combat/BreakerEnemyModifiers.h"
#include "Combat/BreakerModifierComponent.h"
#include "Combat/BreakerSkirmisherEnemy.h"
#include "Combat/BreakerWardenEnemy.h"
#include "Interaction/BreakerNPC.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Math/RandomStream.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "UI/BreakerPlaytestHUD.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "EngineUtils.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"

ABreakerGameMode::ABreakerGameMode()
{
    DefaultPawnClass = ABreakerCharacter::StaticClass();
    static const TCHAR* PlayerBlueprintPath =
        TEXT("/Game/ProjectBreaker/Characters/BP_BreakerCharacter.BP_BreakerCharacter_C");
    if (UClass* PlayerBlueprint = StaticLoadClass(
        ABreakerCharacter::StaticClass(), nullptr, PlayerBlueprintPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
    {
        DefaultPawnClass = PlayerBlueprint;
    }
    HUDClass = ABreakerPlaytestHUD::StaticClass();
    // The supply-crate dwell check runs on the game mode tick.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ABreakerGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    TickSupplyCrate(DeltaSeconds);
}

void ABreakerGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);
    if (bPlaytestTargetsSpawned || !NewPlayer || !NewPlayer->GetPawn() || !GetWorld()) return;
    BuildFieldFrame(NewPlayer->GetPawn());
    // Order matters only in one place: the apron has to exist before anything
    // that stands on it, so SpawnExpandedField runs first now. It used to run
    // last, which is harmless for static meshes and was not for the enemies
    // that ground-snap.
    SpawnExpandedField();
    SpawnBreach();
    SpawnSafeZone();
    SpawnAnchorCamp();
    SpawnPlaytestTargets();
    SpawnMovementCourse();
    SpawnJumpGapRun();
    SpawnCombatEncounter();
    SpawnWorldDressing();
    LogGymSummary();
    BuildCaptureTour();
    ScheduleScreenshots();
}

float ABreakerGameMode::ResolveGroundZ(const APawn* Pawn) const
{
    if (bUseGroundZOverride) return GroundZOverride;
    const UWorld* World = GetWorld();
    if (!World || !Pawn) return Pawn ? Pawn->GetActorLocation().Z - 88.0f : 0.0f;

    // Probe a ring rather than straight down. Straight down from the
    // PlayerStart in Lvl_FirstPerson lands on the template's 210 cm central
    // plinth, which is exactly the mistake the old "location minus 88" made.
    // The LOWEST hit on a ring outside the plinth is the floor the field
    // should be built on.
    const FVector Centre = Pawn->GetActorLocation();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerGroundProbe), false, Pawn);
    float Lowest = TNumericLimits<float>::Max();
    for (int32 Probe = 0; Probe < 8; ++Probe)
    {
        const FVector Start = Centre + FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(Probe * 45.0f, FVector::UpVector) * GroundProbeRadius;
        FHitResult Hit;
        if (World->LineTraceSingleByChannel(Hit, Start + FVector(0, 0, 500.0f), Start - FVector(0, 0, 5000.0f), ECC_Visibility, Params))
        {
            Lowest = FMath::Min(Lowest, static_cast<float>(Hit.ImpactPoint.Z));
        }
    }
    // No hits at all means an open map with no authored floor; the capsule
    // assumption is the only thing left and is correct in that case.
    return Lowest == TNumericLimits<float>::Max() ? Centre.Z - 88.0f : Lowest;
}

void ABreakerGameMode::BuildFieldFrame(const APawn* Pawn)
{
    if (!Pawn) return;
    Frame.Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
    Frame.Right = Pawn->GetActorRightVector().GetSafeNormal2D();
    Frame.SpawnZ = Pawn->GetActorLocation().Z;
    const float GroundZ = ResolveGroundZ(Pawn);
    Frame.Ground = FVector(Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y, GroundZ);
    bFieldFrameSet = true;
    UE_LOG(LogTemp, Display, TEXT("[BreakerGym] field frame: ground z %.0f, spawn z %.0f (%.0f cm of plinth), forward (%.2f, %.2f)"),
        GroundZ, Frame.SpawnZ, Frame.SpawnZ - 88.0f - GroundZ, Frame.Forward.X, Frame.Forward.Y);
}

void ABreakerGameMode::ScheduleScreenshots()
{
    int32 Count = 0;
    if (!FParse::Value(FCommandLine::Get(), TEXT("BreakerScreenshots="), Count) || Count <= 0) return;
    ScreenshotsRemaining = FMath::Clamp(Count, 1, 60);
    ScreenshotIndex = 0;
    NextScreenshotTime = FPlatformTime::Seconds() + FMath::Max(0.1f, ScreenshotFirstDelaySeconds);
    UE_LOG(LogTemp, Display, TEXT("[BreakerCapture] %d screenshots, first at %.1fs, every %.1fs after."),
        ScreenshotsRemaining, ScreenshotFirstDelaySeconds, ScreenshotIntervalSeconds);

    // Shot 0 is the SPAWN EYE VIEW even on a tour: it is the one frame that
    // answers "what does a player see when the level loads", which is the
    // question the owner's complaint is about. The tour starts at shot 1.
    if (TourCameras.Num() > 0)
    {
        if (UWorld* World = GetWorld())
        {
            if (APlayerController* PC = World->GetFirstPlayerController())
            {
                if (APawn* Pawn = PC->GetPawn()) PC->SetViewTarget(Pawn);
            }
        }
    }

    // A CORE ticker, not a world timer. Opening the front end calls
    // SetPause(true), which stops world timers dead, so a world timer here
    // captured nothing at all on any menu screen -- and the menus are half of
    // what needs looking at.
    ScreenshotTickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
        {
            if (FPlatformTime::Seconds() >= NextScreenshotTime)
            {
                NextScreenshotTime = FPlatformTime::Seconds() + FMath::Max(0.1f, ScreenshotIntervalSeconds);
                CaptureScreenshot();
            }
            return ScreenshotsRemaining > 0;
        }), 0.0f);
}

void ABreakerGameMode::BuildCaptureTour()
{
    // -BreakerCaptureTour points the capture at the field instead of at the
    // player's eyes. The spawn view is one composition and a LAYOUT is not
    // visible from inside it; this pass changes the layout, so the harness has
    // to be able to see the layout or it verifies nothing. Vantage points are
    // derived from the same station constants the field is built from, so they
    // move when the field moves.
    if (!GetWorld() || !bFieldFrameSet) return;
    if (!FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureTour"))) return;

    struct FVantage { FVector Location; FRotator Rotation; };
    const float Mid = FieldForwardExtent * 0.4f;
    const TArray<FVantage> Vantages =
    {
        // 1. Straight down over the middle of the field: the plan view.
        { Frame.At(Mid, 0.0f, 16000.0f), FRotator(-89.9f, Frame.Forward.Rotation().Yaw, 0.0f) },
        // 2. Behind and above the camp looking out along the forward axis —
        //    the whole route, camp to arena, in one frame.
        { Frame.At(-FieldRearExtent - 2000.0f, 0.0f, 5200.0f), FRotator(-17.0f, Frame.Forward.Rotation().Yaw, 0.0f) },
        // 3. Standing on the breach crest, the vista the ramp exists to buy.
        { Frame.At(2100.0f, 0.0f, BreachCrestHeight + 170.0f), FRotator(-4.0f, Frame.Forward.Rotation().Yaw, 0.0f) },
        // 4. Oblique over the encounter pocket, to read pocket radius against
        //    the enemies actually standing in it.
        { Frame.At(EncounterPocketDistance - CombatPocketRadius * 2.0f, -CombatPocketRadius, 2600.0f), FRotator(-26.0f, Frame.Forward.Rotation().Yaw + 38.0f, 0.0f) },
        // 5. Down the wall-ride corridor at ride height.
        { Frame.At(EncounterPocketDistance - 3200.0f, FieldHalfExtent * 0.62f, 320.0f), FRotator(-3.0f, Frame.Forward.Rotation().Yaw, 0.0f) },
        // 6. Along the sniper lane from the firing line.
        { Frame.At(RangeFiringLineDistance - 1200.0f, -FieldHalfExtent * 0.62f, 260.0f), FRotator(-2.0f, Frame.Forward.Rotation().Yaw, 0.0f) },
    };

    for (const FVantage& Vantage : Vantages)
    {
        if (ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(Vantage.Location, Vantage.Rotation))
        {
            if (UCameraComponent* Component = Camera->GetCameraComponent())
            {
                Component->SetFieldOfView(90.0f);
            }
            Camera->SetActorLabel(TEXT("Runtime_TourCamera"));
            TourCameras.Add(Camera);
        }
    }
}

void ABreakerGameMode::CaptureScreenshot()
{
    // FScreenshotRequest rather than the HighResShot console command: under
    // -unattended the console exec produced no file and no error, which is the
    // worst possible outcome for a verification tool -- it would have reported
    // success while capturing nothing. bShowUI TRUE is the load-bearing
    // argument; without it the capture omits Slate, and the menus are half of
    // what needs looking at.
    const FString Path = FPaths::ProjectSavedDir() / TEXT("Screenshots") /
        FString::Printf(TEXT("breaker_%02d.png"), ScreenshotIndex);
    FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
    UE_LOG(LogTemp, Display, TEXT("[BreakerCapture] shot %d -> %s"), ScreenshotIndex, *Path);
    ++ScreenshotIndex;

    if (--ScreenshotsRemaining > 0)
    {
        // Cut to the vantage for the NEXT shot now, a whole interval ahead of
        // it. Setting the view target in the same frame as the request races
        // the camera update and would silently shift every image by one -- and
        // "silently" is the word that matters, because a mislabelled vantage
        // is worse than no vantage at all.
        if (TourCameras.Num() > 0)
        {
            if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
            {
                const int32 VantageIndex = (ScreenshotIndex - 1) % TourCameras.Num();
                AActor* Vantage = TourCameras[VantageIndex];
                PC->SetViewTarget(Vantage);
                UE_LOG(LogTemp, Display, TEXT("[BreakerCapture] next shot %d from vantage %d"),
                    ScreenshotIndex, VantageIndex);
            }
        }
        return;
    }

    // Quit on a real-time delay so the last shot finishes writing. Real time
    // again, for the same pause reason.
    const double QuitAt = FPlatformTime::Seconds() + 2.5;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([QuitAt](float) -> bool
    {
        if (FPlatformTime::Seconds() < QuitAt) return true;
        UE_LOG(LogTemp, Display, TEXT("[BreakerCapture] done."));
        FPlatformMisc::RequestExit(false);
        return false;
    }), 0.0f);
}

// One line stating what the gym actually built. This exists for the
// headless smoke run (-BreakerAutoPlay): a log with no gym line means the
// encounter never spawned, which is otherwise indistinguishable from a
// system that simply logs nothing. It is also the fastest way for the owner
// to confirm the area level a session was actually played at.
void ABreakerGameMode::LogGymSummary() const
{
    const UWorld* World = GetWorld();
    if (!World) return;
    int32 Melee = 0;
    int32 Ranged = 0;
    for (TActorIterator<ABreakerEnemy> It(const_cast<UWorld*>(World)); It; ++It)
    {
        if (It->IsRangedForTelemetry()) ++Ranged; else ++Melee;
    }
    int32 Targets = 0;
    for (TActorIterator<ABreakerTargetDummy> It(const_cast<UWorld*>(World)); It; ++It) ++Targets;
    UE_LOG(LogTemp, Display,
        TEXT("[BreakerGym] area level %d | melee %d | ranged %d | target dummies %d"),
        GymAreaLevel, Melee, Ranged, Targets);

    // The stock First Person template geometry is the other half of the "map
    // scope" complaint and it can only be removed in the editor. Measuring it
    // from here is what turns "the template crowds the field" into an
    // actionable delete list: every non-runtime static mesh actor in the map,
    // with the combined footprint it occupies around the spawn.
    FBox TemplateBounds(ForceInit);
    int32 TemplateActors = 0;
    for (TActorIterator<AStaticMeshActor> It(const_cast<UWorld*>(World)); It; ++It)
    {
        if (It->GetActorLabel().StartsWith(TEXT("Runtime_"))) continue;
        const FBox Box = It->GetComponentsBoundingBox(true);
        // Skybox/backdrop meshes are effectively infinite and would swallow the
        // measurement; only the playable shell is interesting here.
        if (Box.GetSize().GetMax() > 50000.0f) continue;
        UE_LOG(LogTemp, Display, TEXT("[BreakerGymTemplate] %s | min (%.0f %.0f %.0f) max (%.0f %.0f %.0f)"),
            *It->GetActorLabel(), Box.Min.X, Box.Min.Y, Box.Min.Z, Box.Max.X, Box.Max.Y, Box.Max.Z);
        TemplateBounds += Box;
        ++TemplateActors;
    }
    if (TemplateActors > 0)
    {
        const FVector Size = TemplateBounds.GetSize();
        const FVector Centre = TemplateBounds.GetCenter();
        UE_LOG(LogTemp, Display,
            TEXT("[BreakerGym] pre-placed (template) static meshes: %d | bounds %.0f x %.0f x %.0f cm | centre (%.0f, %.0f, %.0f) | min (%.0f, %.0f, %.0f) max (%.0f, %.0f, %.0f)"),
            TemplateActors, Size.X, Size.Y, Size.Z, Centre.X, Centre.Y, Centre.Z,
            TemplateBounds.Min.X, TemplateBounds.Min.Y, TemplateBounds.Min.Z,
            TemplateBounds.Max.X, TemplateBounds.Max.Y, TemplateBounds.Max.Z);
    }
    if (const APawn* Pawn = World->GetFirstPlayerController() ? World->GetFirstPlayerController()->GetPawn() : nullptr)
    {
        const FVector P = Pawn->GetActorLocation();
        UE_LOG(LogTemp, Display, TEXT("[BreakerGym] spawn (%.0f, %.0f, %.0f) facing (%.2f, %.2f)"),
            P.X, P.Y, P.Z, Pawn->GetActorForwardVector().X, Pawn->GetActorForwardVector().Y);
    }
}

namespace
{
    // --- Overgrown-Earth palette (O24) -------------------------------------
    // Ground/course blocks: mossy greens and desaturated earth.
    // Ruins/walls: weathered concrete grey-greens.
    // Tech props: dim amber and off-white.
    // Saturated teal is RESERVED for rift/suppression OBJECTS only
    // ("saturated teal is a property of objects, not of damage").
    const FLinearColor PaletteMoss       (0.14f, 0.26f, 0.11f);
    const FLinearColor PaletteFoliage    (0.09f, 0.19f, 0.09f);
    const FLinearColor PaletteDryGrass   (0.30f, 0.32f, 0.15f);
    const FLinearColor PaletteEarth      (0.20f, 0.16f, 0.11f);
    const FLinearColor PaletteConcrete   (0.33f, 0.35f, 0.30f);
    const FLinearColor PaletteStone      (0.24f, 0.26f, 0.23f);
    const FLinearColor PaletteRust       (0.34f, 0.20f, 0.09f);
    const FLinearColor PaletteAmber      (0.46f, 0.29f, 0.08f);
    const FLinearColor PaletteOffWhite   (0.58f, 0.57f, 0.51f);
    const FLinearColor PaletteRiftTeal   (0.03f, 0.72f, 0.66f);   // reserved

    const TCHAR* ShapeCube     = TEXT("/Engine/BasicShapes/Cube.Cube");
    const TCHAR* ShapeSphere   = TEXT("/Engine/BasicShapes/Sphere.Sphere");
    const TCHAR* ShapeCylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
    const TCHAR* ShapeCone     = TEXT("/Engine/BasicShapes/Cone.Cone");

    // The stock basic-shape material exposes a single "Color" vector param, so
    // one dynamic instance per primitive is all the palette needs — no assets.
    void ApplyShapeColor(UStaticMeshComponent* Mesh, const FLinearColor& Color)
    {
        if (!Mesh) return;
        UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (!BaseMaterial) return;
        if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(BaseMaterial, Mesh))
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
            Mesh->SetMaterial(0, Dynamic);
        }
    }

    AStaticMeshActor* SpawnShape(UWorld* World, const TCHAR* ShapePath, const FVector& Location, const FVector& Scale,
        const FRotator& Rotation, const FLinearColor& Color, bool bCollides, const TCHAR* Label)
    {
        if (!World) return nullptr;
        AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Location, Rotation);
        if (!Actor) return nullptr;
        UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, ShapePath));
        Mesh->SetWorldScale3D(Scale);
        ApplyShapeColor(Mesh, Color);
        if (!bCollides) Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        // Dressing never needs to tick or update after placement.
        Mesh->SetMobility(EComponentMobility::Static);
        Actor->SetActorEnableCollision(bCollides);
        Actor->SetActorTickEnabled(false);
        Actor->SetActorLabel(Label);
        return Actor;
    }

    AStaticMeshActor* SpawnGymBlock(UWorld* World, const FVector& Location, const FVector& Scale,
        const FRotator& Rotation = FRotator::ZeroRotator, const FLinearColor& Color = PaletteConcrete)
    {
        return SpawnShape(World, ShapeCube, Location, Scale, Rotation, Color, true, TEXT("Runtime_PlaytestFacility"));
    }

    // A rectangle of ground or wall authored in FIELD units — forward/right
    // extents in cm and a top surface height — rather than in mesh scale.
    // Level-design numbers are dimensions, and a spawner that takes 0.3 when
    // the design says "30 cm thick" is how a derived grammar stops being
    // checkable against the code.
    AStaticMeshActor* SpawnFieldSlab(UWorld* World, const ABreakerGameMode::FFieldFrame& Frame,
        float FwdMin, float FwdMax, float RgtMin, float RgtMax,
        float TopZ, float Thickness, const FLinearColor& Color, const TCHAR* Label, bool bCollides = true)
    {
        const float FwdSize = FMath::Max(FwdMax - FwdMin, 1.0f);
        const float RgtSize = FMath::Max(RgtMax - RgtMin, 1.0f);
        const FVector Centre = Frame.At((FwdMin + FwdMax) * 0.5f, (RgtMin + RgtMax) * 0.5f, TopZ - Thickness * 0.5f);
        return SpawnShape(World, ShapeCube, Centre,
            FVector(FwdSize / 100.0f, RgtSize / 100.0f, Thickness / 100.0f),
            Frame.Forward.Rotation(), Color, bCollides, Label);
    }

    // An inclined slab whose TOP SURFACE runs from (FwdA, HeightA) to
    // (FwdB, HeightB). Ramps are the one piece of level geometry where getting
    // the trigonometry slightly wrong produces a step the player trips on, so
    // the caller states the two endpoints and never a pitch.
    AStaticMeshActor* SpawnFieldRamp(UWorld* World, const ABreakerGameMode::FFieldFrame& Frame,
        float FwdA, float HeightA, float FwdB, float HeightB, float RgtCentre, float Width,
        float Thickness, const FLinearColor& Color, const TCHAR* Label)
    {
        const float Run = FwdB - FwdA;
        const float Rise = HeightB - HeightA;
        const float Length = FMath::Sqrt(Run * Run + Rise * Rise);
        const float PitchDegrees = FMath::RadiansToDegrees(FMath::Atan2(Rise, Run));
        const FRotator Rotation = FRotator(PitchDegrees, Frame.Forward.Rotation().Yaw, 0.0f);
        // Drop the centre by half the thickness along the slab's own normal so
        // the TOP lands on the authored line rather than the mid-plane.
        const FVector Up = Rotation.RotateVector(FVector::UpVector);
        const FVector Centre = Frame.At((FwdA + FwdB) * 0.5f, RgtCentre, (HeightA + HeightB) * 0.5f) - Up * (Thickness * 0.5f);
        return SpawnShape(World, ShapeCube, Centre,
            FVector(Length / 100.0f, Width / 100.0f, Thickness / 100.0f), Rotation, Color, true, Label);
    }

    // Small warm/cool point light bolted onto a prop. Movable because runtime
    // spawns cannot participate in baked lighting; radius and intensity are
    // kept low so the six-light budget stays cheap.
    void AttachPropLight(AActor* Owner, const FVector& RelativeOffset, const FLinearColor& Color, float Intensity, float Radius)
    {
        if (!Owner) return;
        UPointLightComponent* Light = NewObject<UPointLightComponent>(Owner);
        if (!Light) return;
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetupAttachment(Owner->GetRootComponent());
        Light->SetRelativeLocation(RelativeOffset);
        Light->SetLightColor(Color);
        Light->SetIntensity(Intensity);
        Light->SetAttenuationRadius(Radius);
        Light->SetCastShadows(false);
        Light->RegisterComponent();
    }
}

void ABreakerGameMode::SpawnPlaytestTargets()
{
    if (!GetWorld() || !bFieldFrameSet) return;

    // The range moved OUT of the template courtyard. It used to start 1200 cm
    // from the spawn, which put the first two dummies behind the template's own
    // ramps and the third inside the perimeter wall — the "HEALTH 12m" label in
    // the before-shot is pointing at geometry the player cannot shoot through.
    // The firing line is now past the breach, and the four ranges are unchanged
    // relative to it so every falloff reading taken so far still compares.
    const float Line = RangeFiringLineDistance;
    const float Ranges[] = { 1200.0f, 2400.0f, 4500.0f, 2100.0f };
    const float Laterals[] = { -300.0f, 350.0f, 0.0f, -850.0f };
    const EBreakerTargetProfile Profiles[] =
    {
        EBreakerTargetProfile::Health,
        EBreakerTargetProfile::Shielded,
        EBreakerTargetProfile::Armored,
        EBreakerTargetProfile::Moving
    };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Ranges); ++Index)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        const FVector Location = Frame.At(Line + Ranges[Index], Laterals[Index], 120.0f);
        if (ABreakerTargetDummy* Target = GetWorld()->SpawnActor<ABreakerTargetDummy>(ABreakerTargetDummy::StaticClass(), Location, FRotator::ZeroRotator, Params))
        {
            Target->ConfigureProfile(Profiles[Index]);
        }
    }
    bPlaytestTargetsSpawned = true;
}

void ABreakerGameMode::SpawnMovementCourse()
{
    if (!GetWorld() || !bFieldFrameSet) return;
    UWorld* World = GetWorld();

    // --- The rubble stair: the redundant way out of the courtyard ----------
    // Steps at MantleStepHeight so the whole climb is mantle-able and needs no
    // jump at all (master sheet 5.4: the conventional route is never punished).
    // Four steps carry 4 x 145 = 580 cm, which clears the template's 400 cm
    // parapet with the top step landing on the outside apron.
    // Four risers of MantleStepHeight reach 580 cm, which clears the template's
    // 400 cm parapet with the top step landing ON it. Placed to the left of the
    // breach and climbing along the same forward axis, so the courtyard offers
    // two ways out that read differently: a ramp you keep speed on and a stair
    // you climb.
    const float StairRight = -1200.0f;
    for (int32 Step = 0; Step < 4; ++Step)
    {
        const float Top = MantleStepHeight * (Step + 1);
        SpawnFieldSlab(World, Frame, 900.0f + Step * 250.0f, 1150.0f + Step * 250.0f,
            StairRight - 450.0f, StairRight + 450.0f, Top, Top, PaletteEarth, TEXT("Runtime_RubbleStair"));
    }
    // Outer side, sloping back down to the apron. Starts past the wall's inner
    // face so the descent clears the 400 cm crest: at X 1950 it is still at 561.
    SpawnFieldRamp(World, Frame, 1900.0f, MantleStepHeight * 4.0f, 3400.0f, 0.0f,
        StairRight, 900.0f, 60.0f, PaletteEarth, TEXT("Runtime_RubbleStair"));

    // --- Dash reach markers -------------------------------------------------
    // Posts every 500 cm along a clear lane. What they measure is how far one
    // dash carries: a dash floors horizontal speed at 1700 cm/s and holds it
    // while input is held, so the honest reading is posts-per-second, not a
    // fixed distance. Nine posts covers 4000 cm, just under one
    // DashRefreshDistance, so the lane is exactly "how much ground one dash
    // window buys you".
    const float DashLaneRight = -DashCorridorWidth * 1.6f;
    for (int32 Marker = 0; Marker <= 8; ++Marker)
    {
        const float Fwd = RangeFiringLineDistance - 800.0f + Marker * 500.0f;
        SpawnShape(World, ShapeCylinder, Frame.At(Fwd, DashLaneRight, 90.0f),
            FVector(0.12f, 0.12f, 1.8f), FRotator::ZeroRotator,
            Marker % 2 == 0 ? PaletteOffWhite : PaletteStone, false, TEXT("Runtime_DashMarker"));
    }

    // --- Wall-ride corridor -------------------------------------------------
    // Two pairs down the right flank. Length WallRideWallLength (three full
    // 935 cm rides), gap WallRideCorridorWidth. The gap is the fix: the shipped
    // field used 700 cm, and a wall jump leaving at 650 cm/s needs 1.08 s to
    // cross that against roughly 0.85 s of usable air, so the gym's own wall
    // lane could not be chained on the gym's own numbers.
    const float WallLaneRight = FieldHalfExtent * 0.62f;
    for (int32 Pair = 0; Pair < 2; ++Pair)
    {
        // Pairs are separated by one DashRefreshDistance so getting from the
        // end of one ride to the start of the next is a dash decision.
        const float Base = EncounterPocketDistance - 4200.0f + Pair * (WallRideWallLength + DashRefreshDistance);
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const float Lateral = WallLaneRight + (Side == 0 ? -WallRideCorridorWidth : WallRideCorridorWidth) * 0.5f;
            SpawnFieldSlab(World, Frame, Base, Base + WallRideWallLength,
                Lateral - 15.0f, Lateral + 15.0f, WallRideWallHeight, WallRideWallHeight,
                PaletteConcrete, TEXT("Runtime_WallRideWall"));
        }
        // A run-up approach: the entry gate is 450 cm/s of ALONG-WALL speed, so
        // the player needs room to be at sprint before the first wall.
        SpawnFieldSlab(World, Frame, Base - 1400.0f, Base, WallLaneRight - 500.0f, WallLaneRight + 500.0f,
            8.0f, 24.0f, PaletteDryGrass, TEXT("Runtime_WallRideApproach"));
    }

    // --- Flat slide lane ----------------------------------------------------
    // A slide is duration-capped at SlideMaxDuration 1.0 s and entered at
    // sprint, so it covers roughly 1000 cm before it drops under SlideExitSpeed.
    // The lane is exactly that long with a stripe at the midpoint, so the
    // player can see where their slide actually ended instead of guessing.
    const float SlideLaneRight = -FieldHalfExtent * 0.30f;
    SpawnFieldSlab(World, Frame, RangeFiringLineDistance - 1500.0f, RangeFiringLineDistance - 500.0f,
        SlideLaneRight - SprintCorridorWidth * 0.5f, SlideLaneRight + SprintCorridorWidth * 0.5f,
        14.0f, 28.0f, PaletteEarth, TEXT("Runtime_SlideLane"));
    SpawnFieldSlab(World, Frame, RangeFiringLineDistance - 1010.0f, RangeFiringLineDistance - 990.0f,
        SlideLaneRight - SprintCorridorWidth * 0.5f, SlideLaneRight + SprintCorridorWidth * 0.5f,
        16.0f, 28.0f, PaletteAmber, TEXT("Runtime_SlideLane"));

    // --- Watchtowers --------------------------------------------------------
    // Moved out of the courtyard (they used to sit inside the template wall)
    // and onto the range shoulders, where they are what they were named for:
    // a firing perch overlooking the target line, reachable by two jumps off
    // the stack under them.
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const float Lateral = (Side == 0 ? -1.0f : 1.0f) * (CombatPocketRadius + 1000.0f);
        const float Height = 240.0f + Side * 100.0f;
        SpawnFieldSlab(World, Frame, RangeFiringLineDistance + 200.0f, RangeFiringLineDistance + 800.0f,
            Lateral - 300.0f, Lateral + 300.0f, Height, 30.0f, PaletteConcrete, TEXT("Runtime_Watchtower"));
        SpawnFieldSlab(World, Frame, RangeFiringLineDistance + 400.0f, RangeFiringLineDistance + 600.0f,
            Lateral - 100.0f, Lateral + 100.0f, Height - 30.0f, Height - 30.0f, PaletteStone, TEXT("Runtime_Watchtower"));
        // Mantle-height stack so the perch has a conventional route up.
        for (int32 Step = 0; Step < 2; ++Step)
        {
            const float Top = MantleStepHeight * (Step + 1);
            SpawnFieldSlab(World, Frame, RangeFiringLineDistance - 100.0f - Step * 250.0f, RangeFiringLineDistance + 150.0f - Step * 250.0f,
                Lateral - 250.0f, Lateral + 250.0f, Top, Top, PaletteEarth, TEXT("Runtime_Watchtower"));
        }
    }
}

void ABreakerGameMode::SpawnJumpGapRun()
{
    if (!GetWorld() || !bFieldFrameSet) return;
    UWorld* World = GetWorld();

    // Three crossings of one trench, each sized so the verb it needs is the
    // only verb that clears it. Pips in the near kerb count the jumps: one,
    // two, three. This is the piece of the field that makes the derivation
    // FALSIFIABLE — if OneJumpGap cannot be cleared with one jump, the
    // arithmetic in the header is wrong and the doc says so out loud.
    const float TrenchFwd = EncounterPocketDistance + CombatPocketRadius + 2200.0f;
    const float Gaps[] = { OneJumpGap, TwoJumpGap, SwiftThreeJumpGap };
    const float LandingDepth = 1600.0f;
    // Platforms are DashCorridorWidth wide so a player can arrive at speed
    // without threading a needle; a landing narrower than the turn radius is
    // where a gap stops being a jump and starts being a coin flip.
    const float PlatformWidth = DashCorridorWidth;

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Gaps); ++Index)
    {
        // Lanes are separated by two platform widths so an overshoot lands in
        // dirt, not in the neighbouring gap.
        const float Lateral = -PlatformWidth * 1.6f + Index * PlatformWidth * 1.6f;

        // Take-off platform, raised so the trench below reads as a trench.
        SpawnFieldSlab(World, Frame, TrenchFwd - LandingDepth, TrenchFwd,
            Lateral - PlatformWidth * 0.5f, Lateral + PlatformWidth * 0.5f,
            220.0f, 220.0f, PaletteEarth, TEXT("Runtime_JumpGap"));
        // Landing platform at the same height: a flat-to-flat gap is the only
        // one the airtime arithmetic actually describes.
        SpawnFieldSlab(World, Frame, TrenchFwd + Gaps[Index], TrenchFwd + Gaps[Index] + LandingDepth,
            Lateral - PlatformWidth * 0.5f, Lateral + PlatformWidth * 0.5f,
            220.0f, 220.0f, PaletteEarth, TEXT("Runtime_JumpGap"));
        // Amber lips on both edges. A gap you cannot see the edge of is a
        // reaction test, and the whole point of this run is that it is an
        // arithmetic test.
        SpawnFieldSlab(World, Frame, TrenchFwd - 40.0f, TrenchFwd,
            Lateral - PlatformWidth * 0.5f, Lateral + PlatformWidth * 0.5f, 226.0f, 12.0f,
            PaletteAmber, TEXT("Runtime_JumpGap"), false);
        SpawnFieldSlab(World, Frame, TrenchFwd + Gaps[Index], TrenchFwd + Gaps[Index] + 40.0f,
            Lateral - PlatformWidth * 0.5f, Lateral + PlatformWidth * 0.5f, 226.0f, 12.0f,
            PaletteAmber, TEXT("Runtime_JumpGap"), false);
        // Pip stones on the take-off lip: 1 / 2 / 3 jumps.
        for (int32 Pip = 0; Pip <= Index; ++Pip)
        {
            SpawnShape(World, ShapeCube,
                Frame.At(TrenchFwd - 180.0f, Lateral + (Pip - Index * 0.5f) * 140.0f, 260.0f),
                FVector(0.5f, 0.5f, 0.4f), FRotator::ZeroRotator, PaletteAmber, false, TEXT("Runtime_JumpGap"));
        }
    }
    // The trench floor. Deliberately a floor and not a pit: the drop is 220 cm,
    // well under the LandingHeavyFallSpeed threshold, so a failed jump costs
    // the climb back out and nothing else. Falling out of the world is not a
    // teaching tool.
    SpawnFieldSlab(World, Frame, TrenchFwd, TrenchFwd + SwiftThreeJumpGap,
        -PlatformWidth * 2.6f, PlatformWidth * 2.6f, 0.0f, 30.0f, PaletteStone, TEXT("Runtime_JumpGap"));
    // Ramp out of the trench so a miss is recoverable without a jump.
    SpawnFieldRamp(World, Frame, TrenchFwd + SwiftThreeJumpGap, 0.0f, TrenchFwd + SwiftThreeJumpGap + 900.0f, 220.0f,
        PlatformWidth * 2.0f, 700.0f, 40.0f, PaletteEarth, TEXT("Runtime_JumpGap"));
}

void ABreakerGameMode::SpawnBreach()
{
    if (!GetWorld() || !bFieldFrameSet || !bSpawnBreachRamp) return;
    UWorld* World = GetWorld();

    // MEASURED, not assumed (LogGymSummary prints it): Lvl_FirstPerson is a
    // sealed 4000 x 4000 cm courtyard with a continuous parapet — a 200 cm
    // inner course from X 1800 to 2000 and a 200 cm upper course from 1900 to
    // 2000, topping out at 400. There is no doorway. Two base-kit jumps reach
    // 355 cm, so before this the only way into the field the game spawns was
    // to discover a two-stage wall climb, and the field is 85 m of it.
    //
    // A collapsed embankment over the wall is the runtime answer, and it is
    // the right READ as well as the expedient one: overgrown Earth (O24) is
    // exactly a world where the compound wall has been breached and grown over.
    // The proper fix is deleting the wall in the editor — recorded in
    // Docs/Design/Level-Design.md.
    const float Width = SprintCorridorWidth;

    // Ascent. Ends past the wall's outer face at 2100 so the ramp SURFACE
    // clears the 400 cm crest where the crest exists: the upper wall course
    // only spans X 1900-2000, and over that band the ramp runs 433 to 476.
    // Below 1900 the wall is 200 cm and anything clears it.
    // The pitch is atan(520/1200) = 23.4 degrees, well inside the engine's
    // 44.76-degree walkable limit, so it is a run-up and not a climb.
    // Starts at X 900, where the template's own plinth ramp (SM_Ramp11, 500-900)
    // reaches the floor, so the two meet flush instead of one poking through
    // the other.
    SpawnFieldRamp(World, Frame, 900.0f, 0.0f, 2100.0f, BreachCrestHeight, 0.0f, Width, 90.0f,
        PaletteEarth, TEXT("Runtime_Breach"));
    // Descent. 2000 cm of run for 500 cm of drop is 14 degrees, which
    // SlideSlopeAcceleration turns into a genuine downhill slide lane — the
    // gym's old sloped lane, relocated to the one place every route passes
    // through.
    SpawnFieldRamp(World, Frame, 2100.0f, BreachCrestHeight, 4100.0f, 0.0f, 0.0f, Width, 90.0f,
        PaletteEarth, TEXT("Runtime_Breach"));
    // Crest landing, so the top is a place to stand and look rather than a
    // ridge to trip over. This is the vista: the whole field is legible from
    // here, which is what a mouth is FOR.
    SpawnFieldSlab(World, Frame, 2000.0f, 2200.0f, -Width * 0.5f, Width * 0.5f,
        BreachCrestHeight, 120.0f, PaletteStone, TEXT("Runtime_Breach"));
    // Stone edging down both flanks of the ascent. Without it the ramp is a
    // featureless beige wedge filling the spawn view with no depth cue at all —
    // it read as a wall in the first capture, which is the opposite of what a
    // mouth is supposed to say.
    for (int32 Edge = 0; Edge < 2; ++Edge)
    {
        const float Lateral = (Edge == 0 ? -1.0f : 1.0f) * (Width * 0.5f + 30.0f);
        SpawnFieldRamp(World, Frame, 900.0f, 80.0f, 2100.0f, BreachCrestHeight + 80.0f, Lateral, 90.0f, 60.0f,
            PaletteStone, TEXT("Runtime_Breach"));
        SpawnFieldRamp(World, Frame, 2100.0f, BreachCrestHeight + 80.0f, 4100.0f, 80.0f, Lateral, 90.0f, 60.0f,
            PaletteStone, TEXT("Runtime_Breach"));
    }
    // Spill of rubble either side of the crest, so the breach reads as damage
    // rather than as a ramp asset dropped on a wall.
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const float Lateral = (Side == 0 ? -1.0f : 1.0f) * (Width * 0.5f + 200.0f);
        SpawnShape(World, ShapeCube, Frame.At(2050.0f, Lateral, BreachCrestHeight * 0.55f),
            FVector(3.0f, 2.2f, BreachCrestHeight / 100.0f * 0.9f),
            FRotator(0.0f, Side == 0 ? 17.0f : -21.0f, Side == 0 ? 9.0f : -8.0f),
            PaletteConcrete, true, TEXT("Runtime_Breach"));
    }
}

void ABreakerGameMode::SpawnAnchorCamp()
{
    if (!GetWorld() || !bFieldFrameSet) return;
    UWorld* World = GetWorld();

    // The camp now sits ON the courtyard floor rather than 212 cm above it,
    // and it lost its own back wall: the template's parapet already is one, and
    // two walls 200 cm apart is the kind of clutter that makes a 40 m room feel
    // like a 20 m one. Camp centre pulled in from -1400 to -1100 so the 1400 cm
    // plaza fits inside the wall face at -1800 instead of intersecting it.
    const float CampFwd = -1100.0f;
    SpawnFieldSlab(World, Frame, CampFwd - 700.0f, CampFwd + 700.0f, -700.0f, 700.0f,
        16.0f, 32.0f, PaletteEarth, TEXT("Runtime_CampPlaza"));

    if (AStaticMeshActor* Forge = SpawnShape(World, ShapeCube, Frame.At(CampFwd - 400.0f, -450.0f, 110.0f),
        FVector(1.6f, 1.6f, 2.2f), FRotator::ZeroRotator, PaletteRust, true, TEXT("Runtime_Forge")))
    {
        AttachPropLight(Forge, FVector(0, 0, 40.0f), FLinearColor(1.0f, 0.62f, 0.26f), 900.0f, 700.0f);  // Forge glow (light 1/6)
    }
    SpawnShape(World, ShapeCube, Frame.At(CampFwd - 400.0f, 480.0f, 60.0f),
        FVector(2.4f, 1.2f, 1.2f), FRotator::ZeroRotator, PaletteOffWhite, true, TEXT("Runtime_CampProp"));

    // Ammo resupply crate: amber cube beside the quartermaster. Purely a
    // distance trigger evaluated in Tick — no interaction/NPC plumbing.
    SupplyCrateLocation = Frame.At(CampFwd - 200.0f, 480.0f, 0.0f);
    bSupplyCrateSet = true;
    if (AStaticMeshActor* Crate = SpawnShape(World, ShapeCube, SupplyCrateLocation + FVector(0, 0, 55.0f),
        FVector(1.1f, 1.1f, 1.1f), FRotator(0.0f, 12.0f, 0.0f), PaletteAmber, true, TEXT("Runtime_SupplyCrate")))
    {
        AttachPropLight(Crate, FVector(0, 0, 90.0f), FLinearColor(1.0f, 0.68f, 0.28f), 700.0f, 600.0f);  // light 6/6
    }

    ABreakerNPC::SpawnForgeKeeper(World, Frame.At(CampFwd - 320.0f, -450.0f, 100.0f), Frame.Forward.Rotation());
    ABreakerNPC::SpawnQuartermaster(World, Frame.At(CampFwd - 320.0f, 480.0f, 100.0f), Frame.Forward.Rotation());

    // Arena boundary ring, at CombatPocketRadius rather than the old 1400.
    // 1400 was under the 1976 cm a dash-speed orbit needs, so the markers were
    // describing a circle the player could not actually run.
    for (int32 Marker = 0; Marker < 12; ++Marker)
    {
        const float Angle = Marker * 30.0f;
        const FVector Offset = Frame.Forward.RotateAngleAxis(Angle, FVector::UpVector) * CombatPocketRadius;
        SpawnShape(World, ShapeCylinder, Frame.At(ArenaDistance, 0.0f, 130.0f) + Offset,
            FVector(0.22f, 0.22f, 2.6f), FRotator::ZeroRotator, PaletteStone, true, TEXT("Runtime_ArenaMarker"));
    }
}

void ABreakerGameMode::SpawnSafeZone()
{
    if (!GetWorld() || !bFieldFrameSet) return;
    SafeZoneCenter = Frame.Ground;
    bSafeZoneSet = true;

    // Owner feedback: the full-radius teal disc swallowed the spawn area.
    // The boundary now reads as a modest center pad plus a ring of short
    // teal posts at the radius — teal stays on suppression OBJECTS, the
    // ground stays ground.
    AStaticMeshActor* Pad = GetWorld()->SpawnActor<AStaticMeshActor>(SafeZoneCenter + FVector(0, 0, 2.0f), FRotator::ZeroRotator);
    if (Pad)
    {
        UStaticMeshComponent* Mesh = Pad->GetStaticMeshComponent();
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
        Mesh->SetWorldScale3D(FVector(4.0f, 4.0f, 0.04f));
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ApplyShapeColor(Mesh, PaletteRiftTeal * 0.5f);
        Mesh->SetMobility(EComponentMobility::Static);
        Pad->SetActorLabel(TEXT("Runtime_SafeZone"));
    }
    for (int32 Post = 0; Post < 12; ++Post)
    {
        const FVector PostLocation = SafeZoneCenter
            + FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(Post * 30.0f, FVector::UpVector) * SafeZoneRadius
            + FVector(0, 0, 40.0f);
        SpawnShape(GetWorld(), ShapeCylinder, PostLocation, FVector(0.08f, 0.08f, 0.8f), FRotator::ZeroRotator,
            PaletteRiftTeal, false, TEXT("Runtime_SafeZone"));
    }

    // Suppression pylon inside the zone: the second and last teal object.
    if (AStaticMeshActor* Pylon = SpawnShape(GetWorld(), ShapeCylinder,
        SafeZoneCenter + FVector(0, 0, 260.0f), FVector(0.18f, 0.18f, 2.6f), FRotator::ZeroRotator,
        PaletteRiftTeal, false, TEXT("Runtime_SuppressionPylon")))
    {
        AttachPropLight(Pylon, FVector(0, 0, 150.0f), FLinearColor(0.10f, 0.90f, 0.85f), 1400.0f, 900.0f);  // light 2/6
    }
}

bool ABreakerGameMode::IsInSafeZone(const FVector& Location) const
{
    return bSafeZoneSet && FVector::DistSquared2D(Location, SafeZoneCenter) <= FMath::Square(SafeZoneRadius);
}

void ABreakerGameMode::SpawnCombatEncounter()
{
    if (!GetWorld() || !bFieldFrameSet) return;
    UWorld* World = GetWorld();

    // The standing encounter moved from 3500 cm out to the first combat
    // pocket at EncounterPocketDistance. Two reasons, both dimensional:
    //   1. At 3500 the melee pack stood on top of the breach descent — and
    //      before the breach existed, inside a wall the player could not pass.
    //   2. 8500 cm is just under two DashRefreshDistances from the camp, so
    //      the approach is a route with two dash decisions in it rather than a
    //      four-second sprint. The whole complaint was that nothing in this
    //      field is far enough away to be a decision.
    const float LateralOffsets[] = { -450.0f, 0.0f, 450.0f };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        // Inside the pocket, spread across it. The pack occupies the near half
        // so there is CombatPocketRadius of circling room behind them.
        const FVector SpawnLocation = Frame.At(
            EncounterPocketDistance - CombatPocketRadius * 0.5f + Index * 400.0f, LateralOffsets[Index], 120.0f);
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        if (ABreakerEnemy* Enemy = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, Params))
        {
            Enemy->ConfigureEncounter(SpawnLocation, Index * 1.7f);
            // The standing encounter is an area-level-GymAreaLevel area.
            Enemy->SetAreaLevel(GymAreaLevel);
        }
    }

    // One elite anchors the back of the pack: tougher, harder-hitting, and
    // guaranteed Exceptional-or-better drops. It is also the first enemy in the
    // gym to CARRY MODIFIERS — O27 puts difficulty in modifiers rather than
    // trash health, and until this call existed that ruling was implemented in
    // Combat/ and unreachable from a controller.
    const FVector EliteLocation = Frame.At(EncounterPocketDistance + CombatPocketRadius * 0.5f, 0.0f, 120.0f);
    FActorSpawnParameters EliteParams;
    EliteParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    if (ABreakerEnemy* Elite = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), EliteLocation, FRotator::ZeroRotator, EliteParams))
    {
        Elite->ConfigureEncounter(EliteLocation, 0.9f);
        Elite->SetAreaLevel(GymAreaLevel);
        Elite->ConfigureElite();
        GrantModifiers(Elite, ModifierSeedBase);
    }

    // Two LATTICE ranged enemies (Encounter-Design §2.2) flank the pack wide.
    // Placed off to the sides rather than behind the melee so their fire lanes
    // CROSS the ground route the chasers push the player along: the melee
    // enemies deny standing still, the ranged pair deny running in a straight
    // line, and neither problem is solved by the answer to the other.
    //
    // The lateral offset is now CombatPocketRadius rather than a flat 1500, so
    // the pair sits ON the pocket rim: a player entering the pocket is at
    // 2000 cm from each, inside the 900-1900 band's outer edge with the
    // approach still in front of them. RangedSightlineDepth of clear ground
    // behind each one is what lets the retreat gear actually fire.
    const float RangedLateral[] = { -CombatPocketRadius, CombatPocketRadius };
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const FVector SpawnLocation = Frame.At(EncounterPocketDistance - CombatPocketRadius * 0.5f, RangedLateral[Index], 120.0f);
        FActorSpawnParameters RangedParams;
        RangedParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        if (ABreakerRangedEnemy* Ranged = World->SpawnActor<ABreakerRangedEnemy>(
            ABreakerRangedEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, RangedParams))
        {
            Ranged->ConfigureEncounter(SpawnLocation, 0.4f + Index * 1.1f);
            Ranged->SetAreaLevel(GymAreaLevel);
        }
    }
}

void ABreakerGameMode::GrantModifiers(ABreakerEnemy* Enemy, int32 Seed) const
{
    if (!bGrantModifiers || !Enemy) return;

    // The rank the CONTENT authored. ConfigureWithModifiers overwrites it with
    // ModifierBearing, which is a demotion for anything ranked above that, so
    // it is captured and put back. Rank is the single source of truth for what
    // an elite is worth (O27); a modifier roll must not become a second one.
    const EBreakerMonsterRank AuthoredRank = Enemy->GetMonsterRank();
    if (Enemy->ConfigureWithModifiers(Seed) <= 0) return;

    if (Enemy->GetMonsterRank() != AuthoredRank)
    {
        Enemy->SetMonsterRank(AuthoredRank);
        // SetMonsterRank rebuilt the chassis, so max health moved, so the
        // Warded ward is now sized against a number that no longer exists.
        // Re-publishing the same set re-runs ApplyPersistentModifiers against
        // the new health. Copied into a local first because SetModifiers
        // assigns over the very array it would otherwise be reading.
        if (UBreakerEnemyModifierComponent* Modifiers = Enemy->GetModifierComponent())
        {
            const TArray<EBreakerEnemyModifier> Granted = Modifiers->GetModifiers();
            Modifiers->SetModifiers(Granted);
        }
    }
}

void ABreakerGameMode::SpawnWorldDressing()
{
    if (!GetWorld() || !bFieldFrameSet) return;

    // One seed drives every dressing decision so the gym looks identical each
    // run and screenshots stay comparable between playtests.
    FRandomStream Stream(20260812);
    SpawnRuins(Stream);
    SpawnScatteredTech(Stream);
    SpawnOvergrowth(Stream);
}

void ABreakerGameMode::SpawnOvergrowth(FRandomStream& Stream)
{
    // Vegetation clusters: squashed spheres read as bushes, thin tall cones as
    // reeds and saplings. All non-colliding, so movement tests are unaffected.
    //
    // Anchors follow the STATIONS now instead of a hand-picked list that was
    // written when the whole gym fitted in 75 x 50 m. Every one sits on a lane
    // shoulder, because dressing on a shoulder gives the eye something to read
    // speed against and dressing in the middle of a corridor narrows it.
    const FVector Right = Frame.Right;
    const FVector ClusterAnchors[] =
    {
        Frame.At(-1100.0f, -1300.0f),                                        // camp edge
        Frame.At(-1600.0f, 1200.0f),                                         // camp edge
        Frame.At(3000.0f, -SprintCorridorWidth * 1.4f),                      // breach shoulder
        Frame.At(3200.0f, SprintCorridorWidth * 1.5f),                       // breach shoulder
        Frame.At(RangeFiringLineDistance + 900.0f, -CombatPocketRadius),     // range shoulder
        Frame.At(RangeFiringLineDistance + 2600.0f, CombatPocketRadius),     // range shoulder
        Frame.At(EncounterPocketDistance - CombatPocketRadius, -CombatPocketRadius * 1.4f),
        Frame.At(EncounterPocketDistance + CombatPocketRadius, CombatPocketRadius * 1.3f),
        Frame.At(EncounterPocketDistance + CombatPocketRadius + 2200.0f, -DashCorridorWidth * 3.4f),
        Frame.At(ArenaDistance - CombatPocketRadius * 1.5f, CombatPocketRadius * 1.2f),
        Frame.At(ArenaDistance + CombatPocketRadius * 1.4f, -CombatPocketRadius * 1.1f),
        Frame.At(RangeFiringLineDistance, -FieldHalfExtent * 0.62f - 1400.0f) // sniper lane shoulder
    };

    for (const FVector& Anchor : ClusterAnchors)
    {
        const int32 Pieces = Stream.RandRange(5, 7);
        for (int32 Index = 0; Index < Pieces; ++Index)
        {
            const FVector Offset(Stream.FRandRange(-320.0f, 320.0f), Stream.FRandRange(-320.0f, 320.0f), 0.0f);
            const float Yaw = Stream.FRandRange(0.0f, 360.0f);
            const bool bReed = Stream.FRand() < 0.4f;
            const FLinearColor Tint = FMath::Lerp(PaletteFoliage, Stream.FRand() < 0.25f ? PaletteDryGrass : PaletteMoss, Stream.FRand());
            if (bReed)
            {
                const float Height = Stream.FRandRange(1.1f, 2.3f);
                SpawnShape(GetWorld(), ShapeCone, Anchor + Offset + FVector(0, 0, Height * 40.0f),
                    FVector(Stream.FRandRange(0.16f, 0.30f), Stream.FRandRange(0.16f, 0.30f), Height),
                    FRotator(Stream.FRandRange(-9.0f, 9.0f), Yaw, Stream.FRandRange(-9.0f, 9.0f)),
                    Tint, false, TEXT("Runtime_Overgrowth"));
            }
            else
            {
                const float Spread = Stream.FRandRange(0.7f, 1.6f);
                SpawnShape(GetWorld(), ShapeSphere, Anchor + Offset + FVector(0, 0, Stream.FRandRange(6.0f, 26.0f)),
                    FVector(Spread, Spread * Stream.FRandRange(0.75f, 1.2f), Spread * Stream.FRandRange(0.32f, 0.55f)),
                    FRotator(Stream.FRandRange(-7.0f, 7.0f), Yaw, 0.0f),
                    Tint, false, TEXT("Runtime_Overgrowth"));
            }
        }
    }
}

void ABreakerGameMode::SpawnRuins(FRandomStream& Stream)
{
    const FVector Forward = Frame.Forward;
    const FVector Right = Frame.Right;

    // Broken walls: overlapping offset boxes at odd angles, partially sunken,
    // weathered palette. These DO collide — they are playable cover.
    //
    // Spacing is the design content here, not the shapes. The chain from the
    // breach to the encounter pocket is laid out at CoverPitchMax so a player
    // crossing at sprint always has a next piece of cover reachable inside one
    // telegraph-plus-flight window (0.85 + 0.82 = 1.67 s, 1837 cm). Ground with
    // no answer to a telegraph is the failure mode O1 creates by making
    // movement the only active defence.
    const FVector WallAnchors[] =
    {
        Frame.At(4600.0f, -CoverPitchMax * 0.9f),
        Frame.At(4600.0f + CoverPitchMax, CoverPitchMax * 0.7f),
        Frame.At(4600.0f + CoverPitchMax * 2.0f, -CoverPitchMax * 0.5f),
        Frame.At(EncounterPocketDistance - CombatPocketRadius - 400.0f, CoverPitchMax * 0.8f),
        Frame.At(-1900.0f, -900.0f)
    };
    for (int32 Wall = 0; Wall < UE_ARRAY_COUNT(WallAnchors); ++Wall)
    {
        const float BaseYaw = Stream.FRandRange(0.0f, 180.0f);
        for (int32 Segment = 0; Segment < 3; ++Segment)
        {
            const float Height = Stream.FRandRange(1.4f, 3.0f);
            const FVector Slide = Right.RotateAngleAxis(BaseYaw, FVector::UpVector) * (Segment * 320.0f - 320.0f);
            SpawnShape(GetWorld(), ShapeCube,
                WallAnchors[Wall] + Slide + FVector(0, 0, Height * 50.0f - 40.0f),
                FVector(Stream.FRandRange(2.2f, 3.4f), 0.35f, Height),
                FRotator(0.0f, BaseYaw + Stream.FRandRange(-14.0f, 14.0f), Stream.FRandRange(-7.0f, 7.0f)),
                Segment == 1 ? PaletteStone : PaletteConcrete, true, TEXT("Runtime_Ruin"));
        }
    }

    // Collapsed arch: two leaning legs and a fallen span across them. Placed
    // on the forward axis just past the breach, where it is the first thing
    // the field puts in front of the player — a scale reference at 45 m, in
    // the 15-40 m band the art plan says every asset is judged in.
    const FVector ArchBase = Frame.At(4500.0f, -300.0f);
    SpawnShape(GetWorld(), ShapeCube, ArchBase - Right * 400.0f + FVector(0, 0, 200.0f), FVector(0.5f, 0.5f, 4.0f),
        FRotator(0.0f, 0.0f, 11.0f), PaletteConcrete, true, TEXT("Runtime_Ruin"));
    SpawnShape(GetWorld(), ShapeCube, ArchBase + Right * 400.0f + FVector(0, 0, 170.0f), FVector(0.5f, 0.5f, 3.4f),
        FRotator(0.0f, 0.0f, -16.0f), PaletteConcrete, true, TEXT("Runtime_Ruin"));
    SpawnShape(GetWorld(), ShapeCube, ArchBase + FVector(0, 0, 380.0f), FVector(4.6f, 0.6f, 0.45f),
        FRotator(6.0f, 0.0f, -9.0f), PaletteStone, true, TEXT("Runtime_Ruin"));

    // Cracked platform slabs strewn near the arena: low, tilted, mantle-able.
    for (int32 Slab = 0; Slab < 5; ++Slab)
    {
        const float Angle = 34.0f + Slab * 61.0f;
        // Inside the arena rim (CombatPocketRadius) rather than on it: the
        // slabs are footing inside the circle, not a second wall around it.
        const FVector Offset = Forward.RotateAngleAxis(Angle, FVector::UpVector)
            * Stream.FRandRange(CombatPocketRadius * 0.45f, CombatPocketRadius * 0.8f);
        SpawnShape(GetWorld(), ShapeCube,
            Frame.At(ArenaDistance, 0.0f, Stream.FRandRange(10.0f, 40.0f)) + Offset,
            FVector(Stream.FRandRange(2.0f, 3.6f), Stream.FRandRange(1.6f, 2.8f), 0.22f),
            FRotator(Stream.FRandRange(-8.0f, 8.0f), Angle, Stream.FRandRange(-8.0f, 8.0f)),
            Stream.FRand() < 0.5f ? PaletteStone : PaletteConcrete, true, TEXT("Runtime_Ruin"));
    }
}

void ABreakerGameMode::SpawnScatteredTech(FRandomStream& Stream)
{
    // O24's "slight sci-fi": functional, weathered, out of place. Amber and
    // off-white only — teal stays reserved for the pad and the pylon.
    const FVector Forward = Frame.Forward;
    const FVector Right = Frame.Right;

    // 1. Leaning monolith panel at the camp mouth. It is the tallest thing in
    //    the courtyard, so it is the landmark that says which way the mouth is.
    SpawnShape(GetWorld(), ShapeCube, Frame.At(-300.0f, -900.0f, 210.0f),
        FVector(1.6f, 0.22f, 4.2f), FRotator(0.0f, 24.0f, -13.0f), PaletteOffWhite, true, TEXT("Runtime_TechProp"));

    // 2. Generator cylinder with a low amber lamp.
    if (AStaticMeshActor* Generator = SpawnShape(GetWorld(), ShapeCylinder, Frame.At(-500.0f, 900.0f, 70.0f),
        FVector(0.9f, 0.9f, 1.4f), FRotator(0.0f, 0.0f, 4.0f), PaletteRust, true, TEXT("Runtime_TechProp")))
    {
        AttachPropLight(Generator, FVector(0, 0, 90.0f), FLinearColor(1.0f, 0.66f, 0.30f), 800.0f, 650.0f);  // light 3/6
    }

    // 3. Crate stack on the far side of the breach, where the field opens.
    SpawnShape(GetWorld(), ShapeCube, Frame.At(4300.0f, SprintCorridorWidth * 0.9f, 55.0f),
        FVector(1.1f, 1.1f, 1.1f), FRotator(0.0f, 18.0f, 0.0f), PaletteOffWhite, true, TEXT("Runtime_TechProp"));
    SpawnShape(GetWorld(), ShapeCube, Frame.At(4360.0f, SprintCorridorWidth * 0.85f, 150.0f),
        FVector(0.8f, 0.8f, 0.8f), FRotator(0.0f, -31.0f, 5.0f), PaletteRust, true, TEXT("Runtime_TechProp"));

    // 4. Broken antenna ring: half-buried torus faked from tilted cylinders.
    const FVector RingBase = Frame.At(RangeFiringLineDistance + 2200.0f, -CombatPocketRadius * 1.6f);
    for (int32 Segment = 0; Segment < 4; ++Segment)
    {
        SpawnShape(GetWorld(), ShapeCylinder, RingBase + FVector(0, 0, 120.0f + Segment * 18.0f),
            FVector(0.14f, 0.14f, 2.4f), FRotator(0.0f, Segment * 45.0f, 62.0f + Segment * 6.0f),
            PaletteOffWhite, true, TEXT("Runtime_TechProp"));
    }
    if (AStaticMeshActor* Mast = SpawnShape(GetWorld(), ShapeCylinder, RingBase + FVector(0, 0, 200.0f),
        FVector(0.22f, 0.22f, 3.6f), FRotator(0.0f, 0.0f, 9.0f), PaletteRust, true, TEXT("Runtime_TechProp")))
    {
        AttachPropLight(Mast, FVector(0, 0, 160.0f), FLinearColor(1.0f, 0.72f, 0.34f), 600.0f, 550.0f);  // light 4/6
    }

    // 5. Toppled fuel drums near the ruins.
    for (int32 Drum = 0; Drum < 3; ++Drum)
    {
        SpawnShape(GetWorld(), ShapeCylinder,
            Frame.At(4600.0f + CoverPitchMax, -(700.0f + Drum * 180.0f), 40.0f),
            FVector(0.5f, 0.5f, 0.8f), FRotator(Stream.FRandRange(70.0f, 96.0f), Stream.FRandRange(0.0f, 360.0f), 0.0f),
            Drum == 1 ? PaletteOffWhite : PaletteRust, true, TEXT("Runtime_TechProp"));
    }

    // 6. Half-sunken relay cone beside the arena approach, one dim lamp.
    if (AStaticMeshActor* Relay = SpawnShape(GetWorld(), ShapeCone, Frame.At(ArenaDistance - CombatPocketRadius * 1.8f, 950.0f, 60.0f),
        FVector(1.4f, 1.4f, 1.6f), FRotator(0.0f, 0.0f, 17.0f), PaletteAmber, true, TEXT("Runtime_TechProp")))
    {
        AttachPropLight(Relay, FVector(0, 0, 110.0f), FLinearColor(1.0f, 0.70f, 0.32f), 500.0f, 500.0f);  // light 5/6
    }
}

void ABreakerGameMode::RefillPlayerAmmo()
{
    if (!GetWorld()) return;
    APawn* PlayerPawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
    if (UBreakerWeaponComponent* Weapon = PlayerPawn ? PlayerPawn->FindComponentByClass<UBreakerWeaponComponent>() : nullptr)
    {
        Weapon->ResetAmmunition();
    }
}

void ABreakerGameMode::TickSupplyCrate(float DeltaSeconds)
{
    if (!bSupplyCrateSet || !GetWorld()) return;
    APawn* PlayerPawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
    if (!PlayerPawn) return;

    const bool bAtCrate = FVector::DistSquared2D(PlayerPawn->GetActorLocation(), SupplyCrateLocation)
        <= FMath::Square(SupplyCrateRadius);
    if (!bAtCrate)
    {
        SupplyCrateDwell = 0.0f;
        return;
    }
    if (GetWorld()->GetTimeSeconds() - LastSupplyCrateUseTime < SupplyCrateCooldownSeconds) return;

    SupplyCrateDwell += DeltaSeconds;
    if (SupplyCrateDwell >= SupplyCrateDwellSeconds)
    {
        SupplyCrateDwell = 0.0f;
        LastSupplyCrateUseTime = GetWorld()->GetTimeSeconds();
        RefillPlayerAmmo();
    }
}

void ABreakerGameMode::SpawnCombatPocket(float Fwd, float Rgt, FRandomStream& Stream)
{
    UWorld* World = GetWorld();
    if (!World) return;

    // A pocket is a place a movement build can CIRCLE, and that is a
    // measurement, not a mood. Minimum turn radius is v^2/MaxAcceleration:
    // 288 cm at sprint, 688 cm at dash speed. An orbit at exactly the minimum
    // radius is a rail; CombatPocketRadius is twice the dash radius plus body
    // and cover clearance, so the orbit is a choice of line rather than the
    // only line. Everything below is placed as a fraction of that radius, so
    // retuning the radius retunes the pocket instead of breaking it.
    const float BaseYaw = Stream.FRandRange(0.0f, 360.0f);

    // Broken wall arc on the rim. It sweeps ~150 degrees and is deliberately
    // NOT a closed ring: a closed pocket is an arena, and an arena the player
    // cannot leave at speed removes the route choice the pocket exists to
    // create.
    for (int32 Segment = 0; Segment < 5; ++Segment)
    {
        const float Angle = BaseYaw + Segment * 37.0f;
        const FVector Radial = Frame.Forward.RotateAngleAxis(Angle, FVector::UpVector);
        const float Height = Stream.FRandRange(1.6f, 3.2f);
        SpawnShape(World, ShapeCube,
            Frame.At(Fwd, Rgt, Height * 50.0f - 40.0f) + Radial * Stream.FRandRange(CombatPocketRadius * 0.9f, CombatPocketRadius * 1.1f),
            FVector(Stream.FRandRange(2.4f, 3.6f), 0.35f, Height),
            FRotator(0.0f, Angle + 90.0f + Stream.FRandRange(-12.0f, 12.0f), Stream.FRandRange(-6.0f, 6.0f)),
            Segment % 2 == 0 ? PaletteConcrete : PaletteStone, true, TEXT("Runtime_PocketRuin"));
    }

    // Cover inside the pocket, spaced on CoverPitchMax rather than scattered.
    // Four blocks on a 1700 cm ring means a player crossing the pocket always
    // has the next piece inside one telegraph-plus-flight window.
    for (int32 Block = 0; Block < 4; ++Block)
    {
        const float Angle = BaseYaw + 45.0f + Block * 90.0f;
        const FVector Offset = Frame.Forward.RotateAngleAxis(Angle, FVector::UpVector) * (CoverPitchMax * 0.5f);
        SpawnShape(World, ShapeCube, Frame.At(Fwd, Rgt, 55.0f) + Offset,
            FVector(Stream.FRandRange(1.8f, 2.6f), Stream.FRandRange(0.9f, 1.5f), 1.1f),
            FRotator(0.0f, Angle + Stream.FRandRange(-30.0f, 30.0f), 0.0f),
            PaletteStone, true, TEXT("Runtime_PocketCover"));
    }

    // One full-height pillar per pocket: the only thing here that breaks a
    // LATTICE sight line outright (Encounter-Design 3.3 gives the boss arena
    // two for the same reason). Off centre, so it never covers the whole rim.
    SpawnShape(World, ShapeCylinder,
        Frame.At(Fwd, Rgt, 250.0f) + Frame.Forward.RotateAngleAxis(BaseYaw + 200.0f, FVector::UpVector) * (CombatPocketRadius * 0.45f),
        FVector(1.4f, 1.4f, 5.0f), FRotator::ZeroRotator, PaletteConcrete, true, TEXT("Runtime_PocketPillar"));

    for (int32 Bush = 0; Bush < 6; ++Bush)
    {
        const FVector Offset(Stream.FRandRange(-1300.0f, 1300.0f), Stream.FRandRange(-1300.0f, 1300.0f), 0.0f);
        const float Spread = Stream.FRandRange(0.8f, 1.7f);
        SpawnShape(World, ShapeSphere, Frame.At(Fwd, Rgt, Stream.FRandRange(8.0f, 28.0f)) + Offset,
            FVector(Spread, Spread * Stream.FRandRange(0.75f, 1.2f), Spread * Stream.FRandRange(0.32f, 0.55f)),
            FRotator(0.0f, Stream.FRandRange(0.0f, 360.0f), 0.0f),
            FMath::Lerp(PaletteFoliage, PaletteMoss, Stream.FRand()), false, TEXT("Runtime_PocketOvergrowth"));
    }
    SpawnShape(World, ShapeCylinder, Frame.At(Fwd, Rgt + 400.0f, 40.0f),
        FVector(0.5f, 0.5f, 0.8f), FRotator(Stream.FRandRange(70.0f, 96.0f), Stream.FRandRange(0.0f, 360.0f), 0.0f),
        PaletteRust, true, TEXT("Runtime_PocketProp"));
}

void ABreakerGameMode::SpawnExpandedField()
{
    if (!GetWorld() || !bFieldFrameSet) return;
    UWorld* World = GetWorld();

    // THE FIELD, laid out against Docs/Design/Level-Design.md.
    //
    // What was wrong with the previous version, stated plainly because it is
    // the thing the owner has been reporting for weeks:
    //
    //  * The playable room was Lvl_FirstPerson's 4000 x 4000 cm courtyard.
    //    Sprint crosses that in 3.6 s and dash in 2.4 s against a 4.0 s dash
    //    cooldown, so inside the only room the player could reach, the dash
    //    was structurally incapable of being a traversal choice.
    //  * The apron and everything on it was built 212 cm above the real floor,
    //    because the ground plane was taken as "spawn minus a capsule" and the
    //    spawn is on a 210 cm plinth.
    //  * The additions were islands: three pockets, a lane and a wall pair
    //    scattered across 180 m of featureless flat with nothing between them,
    //    so the field read as a small box next to a car park.
    //
    // What replaces it is a route with STATIONS, each at least one
    // DashRefreshDistance from the last, on the real floor, entered through a
    // breach in the courtyard wall. Numbers come from the constants in the
    // header; nothing here is a literal that is not either a fraction of one
    // of them or a piece of dressing.
    //
    // Its own seed stream, offset from the dressing seed, so both stay
    // deterministic and independent.
    FRandomStream Stream(20260812 + 101);

    // --- 1. Ground --------------------------------------------------------
    // Four big slabs, not a tile grid. The template Floor already covers
    // +/-2000 and its top is exactly at the ground plane, so the apron is
    // authored as the rectangle AROUND it: abutting, never overlapping, no
    // coplanar z-fighting and no step at the seam. It also drops the actor
    // count from 81 tiles to 4.
    const float Back = -FieldRearExtent;
    const float Front = FieldForwardExtent;
    const float Side = FieldHalfExtent;
    const float Shell = 2000.0f;   // the template courtyard half-extent, measured
    SpawnFieldSlab(World, Frame, Back, -Shell, -Side, Side, 0.0f, 40.0f, PaletteEarth, TEXT("Runtime_FieldApron"));
    SpawnFieldSlab(World, Frame, Shell, Front, -Side, Side, 0.0f, 40.0f, PaletteEarth, TEXT("Runtime_FieldApron"));
    SpawnFieldSlab(World, Frame, -Shell, Shell, -Side, -Shell, 0.0f, 40.0f, PaletteEarth, TEXT("Runtime_FieldApron"));
    SpawnFieldSlab(World, Frame, -Shell, Shell, Shell, Side, 0.0f, 40.0f, PaletteEarth, TEXT("Runtime_FieldApron"));

    // Tint patches: non-colliding flat plates that break 250 x 220 m of one
    // colour. Purely so the eye has something to judge speed against — a
    // featureless plane is a large part of why a big field can still read as
    // nothing (O24 dressing, no gameplay meaning).
    // Deliberately SMALL and numerous. The first version used 900-2600 cm
    // plates and the plan view read as farmland — a patch the size of a combat
    // pocket is a landmark, not a texture, and it lies about the scale of the
    // thing next to it. Scrub-sized plates give the eye optical flow to judge
    // speed against without ever being mistaken for geometry.
    for (int32 Patch = 0; Patch < 200; ++Patch)
    {
        const float Fwd = Stream.FRandRange(Back, Front);
        const float Rgt = Stream.FRandRange(-Side, Side);
        if (FMath::Abs(Fwd) < Shell && FMath::Abs(Rgt) < Shell) continue;
        const float SizeX = Stream.FRandRange(320.0f, 1100.0f);
        const float SizeY = Stream.FRandRange(320.0f, 1100.0f);
        SpawnShape(World, ShapeCube, Frame.At(Fwd, Rgt, 2.0f),
            FVector(SizeX / 100.0f, SizeY / 100.0f, 0.04f),
            FRotator(0.0f, Stream.FRandRange(0.0f, 360.0f), 0.0f),
            FMath::Lerp(PaletteEarth, Stream.FRand() < 0.3f ? PaletteDryGrass : PaletteMoss, Stream.FRandRange(0.35f, 1.0f)),
            false, TEXT("Runtime_FieldPatch"));
    }

    // --- 2. The forward route ---------------------------------------------
    // Shoulder ruins flanking the axis from the breach exit to the arena. They
    // are placed at DashCorridorWidth * 1.5 from the centreline on each side,
    // so the main route is 4800 cm wide: three sprint corridors, or one dash
    // corridor with a corridor of clear ground either side of it. A route this
    // wide is not a corridor at all, which is deliberate — the corridors in
    // this field are the SIDE lanes, and the spine is open ground you choose a
    // line across.
    const float SpineHalf = DashCorridorWidth * 1.5f;
    for (int32 Marker = 0; Marker < 9; ++Marker)
    {
        const float Fwd = 4600.0f + Marker * DashRefreshDistance * 0.45f;
        if (Fwd > ArenaDistance - CombatPocketRadius) break;
        for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
        {
            const float Lateral = (SideIndex == 0 ? -1.0f : 1.0f) * SpineHalf;
            const float Height = Stream.FRandRange(1.2f, 2.6f);
            SpawnShape(World, ShapeCube, Frame.At(Fwd + Stream.FRandRange(-500.0f, 500.0f), Lateral + Stream.FRandRange(-300.0f, 300.0f), Height * 50.0f - 30.0f),
                FVector(Stream.FRandRange(2.0f, 3.4f), 0.4f, Height),
                FRotator(0.0f, Stream.FRandRange(0.0f, 180.0f), Stream.FRandRange(-8.0f, 8.0f)),
                Marker % 2 == 0 ? PaletteConcrete : PaletteStone, true, TEXT("Runtime_SpineRuin"));
        }
    }

    // --- 3. Combat pockets -------------------------------------------------
    // Three, all at least one DashRefreshDistance apart so moving between them
    // is a route decision. The first is the standing encounter's ground.
    SpawnCombatPocket(EncounterPocketDistance, 0.0f, Stream);
    SpawnCombatPocket(EncounterPocketDistance + DashRefreshDistance, FieldHalfExtent * 0.55f, Stream);
    SpawnCombatPocket(RangeFiringLineDistance + DashRefreshDistance, -FieldHalfExtent * 0.55f, Stream);
    // Fourth pocket IS the elite arena: same radius, same grammar, marked with
    // the ring in SpawnAnchorCamp and reused by wave mode.
    SpawnCombatPocket(ArenaDistance, 0.0f, Stream);

    // --- 4. Sniper sightline lane -----------------------------------------
    // Runs down the left flank with three distance markers. Its width is
    // DashCorridorWidth (a lane the player is expected to move fast down) and
    // its markers sit at 30 / 60 / 90 m from the firing line. It starts at the
    // range's firing line rather than at the spawn, so the numbers on the
    // posts are the numbers the target dummies are at.
    const float LaneRight = -FieldHalfExtent * 0.62f;
    const float LaneStart = RangeFiringLineDistance - 1500.0f;
    const float MarkerDistances[] = { 3000.0f, 6000.0f, 9000.0f };   // 30 / 60 / 90 m
    for (int32 Marker = 0; Marker < UE_ARRAY_COUNT(MarkerDistances); ++Marker)
    {
        const float Fwd = LaneStart + MarkerDistances[Marker];
        // Post height scales with range so the far marker still subtends
        // something readable through a scope.
        const float PostHeight = 3.0f + Marker * 1.0f;
        SpawnShape(World, ShapeCylinder, Frame.At(Fwd, LaneRight, PostHeight * 50.0f),
            FVector(0.3f, 0.3f, PostHeight), FRotator::ZeroRotator, PaletteOffWhite, true, TEXT("Runtime_RangeMarker"));
        SpawnShape(World, ShapeCube, Frame.At(Fwd, LaneRight, PostHeight * 100.0f + 30.0f),
            FVector(1.6f, 0.3f, 0.6f), FRotator::ZeroRotator, PaletteAmber, true, TEXT("Runtime_RangeMarker"));
        for (int32 Pip = 0; Pip <= Marker; ++Pip)
        {
            SpawnShape(World, ShapeCube, Frame.At(Fwd, LaneRight + Pip * 120.0f - 60.0f, 25.0f),
                FVector(0.7f, 0.7f, 0.5f), FRotator::ZeroRotator, PaletteAmber, true, TEXT("Runtime_RangeMarker"));
        }
    }
    // Kerbs at DashCorridorWidth so the lane reads as a lane. Low enough that
    // they never block a shot down it, which is the whole point of a sightline.
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Lateral = LaneRight + (SideIndex == 0 ? -1.0f : 1.0f) * DashCorridorWidth * 0.5f;
        SpawnFieldSlab(World, Frame, LaneStart, LaneStart + 10000.0f,
            Lateral - 30.0f, Lateral + 30.0f, 45.0f, 45.0f, PaletteStone, TEXT("Runtime_SniperLaneKerb"));
    }
    // One ranged-band pocket halfway down the lane: RangedSightlineDepth of
    // clear ground behind a single piece of hard cover, which is the minimum
    // geometry a LATTICE needs to use its whole 900-1900 band instead of
    // backing into a kerb.
    SpawnShape(World, ShapeCube, Frame.At(LaneStart + RangedSightlineDepth, LaneRight + 500.0f, 150.0f),
        FVector(2.6f, 0.5f, 3.0f), FRotator(0.0f, 8.0f, -4.0f), PaletteConcrete, true, TEXT("Runtime_LaneCover"));
}


void ABreakerGameMode::StartNextWave()
{
    if (!GetWorld() || IsWaveActive()) return;
    APawn* PlayerPawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
    if (!PlayerPawn) return;

    // Wave clear = full restock. Reaching here with a wave already started
    // means the previous wave is empty (IsWaveActive() gated above), so the
    // player begins every wave topped up. Answers "no way to regain ammo"
    // at the coarse grain; kill drops and the camp crate cover the rest.
    if (CurrentWave > 0) RefillPlayerAmmo();

    ++CurrentWave;
    WaveEnemies.RemoveAll([](const TObjectPtr<ABreakerEnemy>& Enemy) { return !IsValid(Enemy); });

    const FVector Origin = PlayerPawn->GetActorLocation();
    const FVector Forward = PlayerPawn->GetActorForwardVector().GetSafeNormal2D();
    // Wave mode deliberately spawns around the PLAYER rather than at the
    // authored arena: the instrument has to work wherever a playtest happens
    // to be standing. What changed is the distance, which is now derived
    // instead of the old SafeZoneRadius + 4200. One DashRefreshDistance out
    // means the pack is exactly one dash-cooldown of ground away — inside
    // Encounter-Design 5.2's 1500-4000 cm spawn band at the near packs and
    // still far enough that nothing materialises in the player's face.
    const FVector ArenaCenter = Origin + Forward * DashRefreshDistance;
    // Dense packs by design: AoE, on-death chains, and multikill procs need
    // crowds to feel like anything. Clusters of ~4 around the arena ring.
    const int32 EnemyCount = FMath::Min(4 + CurrentWave * 3, 24);
    const bool bEliteWave = CurrentWave % 3 == 0;

    // LATTICE ranged enemies join from wave 2 and climb to the hard cap of 3
    // live at once (Encounter-Design §5.3: "four converging projectile sources
    // removes all safe ground; this is the single most dangerous scaling
    // knob"). They come OUT OF the melee budget rather than on top of it, so
    // pack density and the TTK sample size are unchanged — what changes is the
    // kind of pressure, not the amount.
    const int32 RangedCount = FMath::Clamp(CurrentWave / 2, 0, 3);
    const int32 MeleeCount = FMath::Max(EnemyCount - RangedCount, 1);

    for (int32 Index = 0; Index < MeleeCount; ++Index)
    {
        const int32 Pack = Index / 4;
        const float PackAngle = 360.0f * Pack / FMath::Max(1, (MeleeCount + 3) / 4);
        // Packs sit on the pocket rim rather than at a flat 1100 cm, so the
        // ring the player circles is the same radius everywhere in the field.
        const FVector PackCenter = ArenaCenter + FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(PackAngle, FVector::UpVector) * (CombatPocketRadius * 0.55f);
        const FVector SpawnLocation = PackCenter + FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(Index * 90.0f, FVector::UpVector) * 160.0f;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        if (ABreakerEnemy* Enemy = GetWorld()->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, Params))
        {
            Enemy->ConfigureEncounter(SpawnLocation, Index * 1.3f);
            // Later waves climb in level so drops and TTK data climb too.
            Enemy->ConfigureWave(GetAreaLevelForWave(CurrentWave));
            if (bEliteWave && Index == 0)
            {
                Enemy->ConfigureElite();
                // Seeded on the WAVE, so wave 3 is the same Champion every run
                // and a TTK sample taken across two sessions compares.
                GrantModifiers(Enemy, ModifierSeedBase + CurrentWave * 7919);
            }
            WaveEnemies.Add(Enemy);
        }
    }

    // Ranged support sits a ring further out and spread evenly around the
    // arena, so the melee packs push the player ACROSS the ranged fire lanes
    // instead of away from them. Never promoted to elite: the elite is already
    // the melee anchor, and two things to read at once is one too many.
    for (int32 Index = 0; Index < RangedCount; ++Index)
    {
        const float Angle = 360.0f * Index / FMath::Max(1, RangedCount) + 45.0f;
        const FVector SpawnLocation = ArenaCenter
            + FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(Angle, FVector::UpVector) * CombatPocketRadius;
        FActorSpawnParameters RangedParams;
        RangedParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        if (ABreakerRangedEnemy* Ranged = GetWorld()->SpawnActor<ABreakerRangedEnemy>(
            ABreakerRangedEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, RangedParams))
        {
            Ranged->ConfigureEncounter(SpawnLocation, Index * 0.9f);
            Ranged->ConfigureWave(GetAreaLevelForWave(CurrentWave));
            WaveEnemies.Add(Ranged);
        }
    }
}

int32 ABreakerGameMode::GetAreaLevelForWave(int32 WaveIndex) const
{
    // The gym's area level climbs with the wave. This is CONTENT escalation:
    // wave 5 is a harder area than wave 1 regardless of who is standing in it.
    const int32 Level = GymAreaLevel + FMath::Max(WaveIndex, 0) * FMath::Max(AreaLevelPerWave, 0);
    return UBreakerMonsterChassisLibrary::ClampAreaLevel(Level);
}

int32 ABreakerGameMode::GetWaveEnemiesAlive() const
{
    int32 Alive = 0;
    for (const TObjectPtr<ABreakerEnemy>& Enemy : WaveEnemies)
    {
        if (IsValid(Enemy) && Enemy->GetEnemyStateLabel() != TEXT("DEAD")) ++Alive;
    }
    return Alive;
}

void ABreakerGameMode::ResetPlaytestTargets()
{
    if (!GetWorld()) return;
    for (TActorIterator<ABreakerTargetDummy> It(GetWorld()); It; ++It) It->Destroy();
    for (TActorIterator<ABreakerEnemy> It(GetWorld()); It; ++It) It->Destroy();
    bPlaytestTargetsSpawned = false;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get(); PC && PC->GetPawn())
        {
            SpawnPlaytestTargets();
            SpawnCombatEncounter();
            break;
        }
    }
}
