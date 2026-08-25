#include "Game/BreakerGameMode.h"

#include "Game/BreakerHubBuilder.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerWorldBasics.h"
#include "GameFramework/PlayerStart.h"
#include "Interaction/BreakerTravelPoint.h"

#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerTargetDummy.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerMonsterChassis.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Combat/BreakerBossEnemy.h"
#include "Combat/BreakerEnemyModifiers.h"
#include "Combat/BreakerModifierComponent.h"
#include "Combat/BreakerSkirmisherEnemy.h"
#include "Combat/BreakerAlteredEnemy.h"
#include "Combat/BreakerWardenEnemy.h"
#include "Interaction/BreakerNPC.h"
#include "Playtest/BreakerKillTelemetryComponent.h"
#include "Playtest/BreakerPlaytestComponent.h"
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
#include "Combat/BreakerZoneActor.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerPlaytestHUD.h"
#include "UI/BreakerUIStyle.h"
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

void ABreakerGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    // A console command registered against a game-mode instance outlives the
    // world unless it is unregistered: the next PIE session's `Breaker.Boss`
    // would call through a dangling this.
    if (BossConsoleCommand)
    {
        IConsoleManager::Get().UnregisterConsoleObject(BossConsoleCommand);
        BossConsoleCommand = nullptr;
    }
    if (EffectProbeConsoleCommand)
    {
        IConsoleManager::Get().UnregisterConsoleObject(EffectProbeConsoleCommand);
        EffectProbeConsoleCommand = nullptr;
    }
    Super::EndPlay(Reason);
}

void ABreakerGameMode::TeleportPawnToHub(APawn* Pawn)
{
    if (!Pawn) return;
    if (!bHubBuilt)
    {
        // Loud rather than a silent no-op: a PLAY button that appears to do
        // nothing is the failure this whole pass exists to remove. If the hub
        // was never built the player stays where they are and the log says why.
        UE_LOG(LogTemp, Warning,
            TEXT("TeleportPawnToHub: the hub has not been built, so there is nowhere to go. ")
            TEXT("The pawn was left where it was."));
        return;
    }
    // The gate-side arrival spot, never HubOrigin: the origin is the plaza
    // centre, and the centre holds the colliding landmark obelisk — the old
    // +120 teleport put the player INSIDE it.
    Pawn->TeleportTo(HubArrival.GetLocation(), HubArrival.Rotator());
    if (AController* Controller = Pawn->GetController())
    {
        Controller->SetControlRotation(HubArrival.Rotator());
    }
}

void ABreakerGameMode::HandleHubTravelSelected(FName DestinationId, APawn* RequestingPawn)
{
    if (!RequestingPawn) return;
    // TRAVEL IS A LEVEL LOAD NOW, not a teleport. It was a teleport because
    // there was one map and both places were in it; with three maps the
    // destination does not exist until it is loaded.
    if (UBreakerGameInstance* Session = GetGameInstance<UBreakerGameInstance>())
    {
        Session->PendingDestinationId = DestinationId;
    }
    if (DestinationId == ABreakerTravelPoint::HubDestinationId)
    {
        UBreakerGameInstance::TravelTo(this, FName(UBreakerGameInstance::AnchorMapName()));
        return;
    }
    if (DestinationId == ABreakerTravelPoint::GymDestinationId)
    {
        UBreakerGameInstance::TravelTo(this, FName(UBreakerGameInstance::GymMapName()));
        return;
    }
    // Any other id is refused rather than guessed at. The old teleport that
    // stood here is gone with the one-map world it belonged to: the gym is a
    // separate level now, so arriving there is a load, and the load is what
    // builds it.
    UE_LOG(LogTemp, Warning, TEXT("HandleHubTravelSelected: no map is registered for destination '%s'."),
        *DestinationId.ToString());
}

AActor* ABreakerGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    if (AActor* Authored = Super::ChoosePlayerStart_Implementation(Player))
    {
        return Authored;
    }
    // Z 112: the pawn capsule's half-height is 88, so feet land at 24 — the
    // same "capsule assumption" plane ResolveGroundZ falls back to in a map
    // with no floor, which is exactly the map this branch exists for. The
    // apron / plaza / boot floor all get built at that plane in the same
    // frame, so the pawn stands on ground it arrived with.
    UWorld* World = GetWorld();
    if (!World) return nullptr;
    APlayerStart* Fallback = World->SpawnActor<APlayerStart>(
        APlayerStart::StaticClass(), FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 112.0f)));
    if (Fallback)
    {
        UE_LOG(LogTemp, Log, TEXT("[BreakerMap] no authored PlayerStart; runtime fallback spawned at origin."));
    }
    return Fallback;
}

void ABreakerGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);
    if (bPlaytestTargetsSpawned || !NewPlayer || !NewPlayer->GetPawn() || !GetWorld()) return;

    // Light to see by, for every map role. Runs before the front-end early
    // return on purpose: the title screen floats over a real (if minimal)
    // world now, not over a black void. A map with an authored directional
    // light (Lvl_FirstPerson) suppresses this entirely.
    UBreakerWorldBasics::EnsureWorldLighting(GetWorld());

    // WHAT THIS MAP IS FOR. Until tonight there was one map, so this function
    // unconditionally built the entire gym — which is exactly why the owner
    // reported "loading in still takes you to the game": the front end was a
    // widget drawn over a gym that had already been constructed and was
    // already ticking underneath it.
    //
    // The front end builds NOTHING. That is the whole point of the split, and
    // it is why this returns before BuildFieldFrame rather than after: the
    // frame is derived from the pawn and every spawner hangs off it, so an
    // early return here is the one place that guarantees no field exists.
    if (UBreakerGameInstance::IsFrontEndMap(this))
    {
        // "Nothing" still needs a floor: the map is an empty shell and the
        // pawn under the title menu was falling through it. Top surface at
        // the pawn's feet, same plane the fallback PlayerStart assumed.
        if (const APawn* Pawn = NewPlayer->GetPawn())
        {
            UBreakerWorldBasics::EnsureBootFloor(GetWorld(),
                Pawn->GetActorLocation() - FVector(0.0f, 0.0f, 88.0f));
        }
        bPlaytestTargetsSpawned = true;
        // The capture harness works on the front end too. Without this a
        // -BreakerScreenshots run of the shipped boot map never schedules its
        // exit and hangs forever — which is also why no automated run ever
        // photographed the title screen the game actually boots into.
        ScheduleScreenshots();
        UE_LOG(LogTemp, Log, TEXT("[BreakerMap] front end — no field built."));
        return;
    }

    // THE RIFT DEFINITION OWNS THE AREA LEVEL when a travel carried one:
    // adopted once per map build, before anything derives from it. The
    // EditAnywhere GymAreaLevel remains the dev fallback for sessions with
    // no chosen rift — PIE drop-ins, the capture harness — which is the wall
    // between "a tunable on the game mode" and "a property of the place".
    if (const UBreakerGameInstance* Session = GetGameInstance<UBreakerGameInstance>())
    {
        if (Session->PendingRift.IsSet())
        {
            GymAreaLevel = Session->PendingRift.EffectiveAreaLevel();
        }
    }

    BuildFieldFrame(NewPlayer->GetPawn());

    // The Anchor builds the hub and stops. No gym field, no encounter, no
    // waves — a social space with a gate, which is what a hub is.
    if (UBreakerGameInstance::IsAnchorMap(this))
    {
        HubOrigin = Frame.Ground;
        bHubBuilt = true;
        const FTransform HubFrame(Frame.Forward.Rotation(), HubOrigin);
        if (ABreakerTravelPoint* HubTravel = UBreakerHubBuilder::BuildHub(GetWorld(), HubFrame))
        {
            HubTravel->ExcludedDestinationId = ABreakerTravelPoint::HubDestinationId;
            HubTravel->OnDestinationSelected.AddUObject(this, &ABreakerGameMode::HandleHubTravelSelected);
        }
        // THE HUB IS BUILT AROUND THE ARRIVING PAWN, which means the pawn is
        // standing at plaza centre — inside the landmark obelisk BuildHub just
        // spawned there. Move them to the gate-side arrival spot immediately,
        // in the same frame, before physics gets an opinion.
        HubArrival = UBreakerHubBuilder::ArrivalTransform(HubFrame);
        TeleportPawnToHub(NewPlayer->GetPawn());
        bPlaytestTargetsSpawned = true;
        // Same reason as the front end: a screenshot run of the Anchor must
        // schedule its exit, or the harness can never photograph the hub.
        ScheduleScreenshots();
        UE_LOG(LogTemp, Log, TEXT("[BreakerMap] anchor — hub built, no gym."));
        return;
    }

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
    // THE HUB, and its travel point back to this gym. Placed BEHIND the safe
    // ring, on the opposite side from the encounter, so it cannot overlap the
    // field SpawnExpandedField just built or the arena the combat spawns use.
    //
    // Reachability (O40c): a hub nobody can walk to is the same defect as an
    // ability nobody can equip. This call, and the delegate bind under it, are
    // the whole in-game path — the travel point deliberately does not move
    // anyone itself (it has no idea where the gym is), so the game mode owns
    // the teleport because the game mode is what knows Frame.Ground.
    // NO HUB IN THE GYM ANY MORE. Owner: "the gym is still attatched to the
    // anchor" — and it was, literally: the hub was built 6000 cm from the gym's
    // origin IN THE SAME WORLD, so the two were one continuous space a player
    // could walk between. They are separate maps now, and the gym builds only
    // the gym.
    // The return gate, beside the safe pad where a player who has finished a
    // run is already standing. Travel was one-way without it.
    if (ABreakerTravelPoint* GymTravel = GetWorld()->SpawnActor<ABreakerTravelPoint>(
            ABreakerTravelPoint::StaticClass(), FTransform(Frame.At(600.0f, -600.0f, 0.0f))))
    {
        GymTravel->ExcludedDestinationId = ABreakerTravelPoint::GymDestinationId;
        GymTravel->OnDestinationSelected.AddUObject(this, &ABreakerGameMode::HandleHubTravelSelected);
    }
    LogGymSummary();
    BuildCaptureTour();
    ScheduleScreenshots();

    // THE BOSS KEY. F5, because the playtest keys F1-F4 and the F talk key all
    // live on ABreakerCharacter (Characters/), which this lane does not own.
    // Binding onto the PLAYER CONTROLLER's input component reaches the same
    // keyboard without touching that file: the pawn's component sits above the
    // controller's on the input stack, so a key the pawn does not claim falls
    // through to here, and F5 is claimed by nothing.
    if (NewPlayer->InputComponent)
    {
        NewPlayer->InputComponent->BindKey(EKeys::F5, IE_Pressed, this, &ABreakerGameMode::SpawnBossTest);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[BreakerGym] no controller input component; the F5 boss key is unavailable. Use Breaker.Boss."));
    }
    // -BreakerBossOnStart spawns the Field Marshal during the gym build, so the
    // capture harness can PHOTOGRAPH it. Without this the boss is only
    // reachable by a key press, and a headless run cannot press a key — which
    // would leave the one archetype most worth looking at as the one archetype
    // nobody has looked at. Dev-only by construction: a command-line switch
    // cannot be reached from a shipped build.
    if (FParse::Param(FCommandLine::Get(), TEXT("BreakerBossOnStart")))
    {
        SpawnBossTest();
        LogGymSummary();
    }
    if (!BossConsoleCommand)
    {
        BossConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
            TEXT("Breaker.Boss"),
            TEXT("Spawns THE FIELD MARSHAL at the elite arena (Encounter-Design 3)."),
            FConsoleCommandDelegate::CreateUObject(this, &ABreakerGameMode::SpawnBossTest));
    }
    // -BreakerEffectProbe: the Phase B proof. One glow, fixed spot in the
    // spawn view, fixed clock, placed during the gym build so the capture
    // cadence (first frame at 6.0 s, then every 2.0 s) straddles its death:
    // it must be IN the first frame and GONE from the third. Same dev-only
    // construction as -BreakerBossOnStart.
    if (FParse::Param(FCommandLine::Get(), TEXT("BreakerEffectProbe")))
    {
        SpawnEffectProbe();
    }
    if (!EffectProbeConsoleCommand)
    {
        EffectProbeConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
            TEXT("Breaker.EffectProbe"),
            TEXT("Places one 7 s test glow ahead of the gym spawn (ability-effect renderer probe)."),
            FConsoleCommandDelegate::CreateUObject(this, &ABreakerGameMode::SpawnEffectProbe));
    }
}

void ABreakerGameMode::SpawnEffectProbe()
{
    UWorld* World = GetWorld();
    if (!World || !bFieldFrameSet) return;
    if (!EffectProbeRenderer)
    {
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        EffectProbeRenderer = World->SpawnActor<ABreakerEffectRenderer>(
            ABreakerEffectRenderer::StaticClass(), FTransform::Identity, Params);
    }
    if (!EffectProbeRenderer) return;

    // 5 m ahead of the field origin at chest height: dead centre of the spawn
    // view, close enough that a 50 cm sphere is unmistakably in frame. Cyan,
    // the player-system token — this pool exists for the player's abilities.
    // Duration 7.0 s against the 6.0/8.0/10.0 s capture frames: alive in the
    // first, dying or dead at the second, unarguably gone by the third. All
    // O2 PLACEHOLDER except the 7.0, which is derived from the cadence.
    const FVector ProbeSpot = Frame.At(500.0f, 0.0f, 140.0f);
    BreakerFX::FEffectTiming Timing;
    Timing.DurationSeconds = 7.0f;
    Timing.FadeInSeconds = 0.15f;
    Timing.FadeOutSeconds = 0.5f;
    EffectProbeRenderer->AddGlow(ProbeSpot, 50.0f, BreakerUI::Cyan, 6.0f, Timing);
    // The log half of the proof: a headless reader greps this line for WHERE
    // and UNTIL WHEN, then reads the screenshots for whether the world agreed.
    UE_LOG(LogTemp, Log, TEXT("[BreakerGym] effect probe: glow at (%.0f, %.0f, %.0f) for %.1f s."),
        ProbeSpot.X, ProbeSpot.Y, ProbeSpot.Z, Timing.DurationSeconds);

    // A Rot-shaped zone beside the glow, on the same 7.0 s straddle clock:
    // the zone's own presentation plus the rim the effect renderer draws for
    // it, photographed alive in the first frame and expired out of the later
    // ones. Geometry is Rot's authored footprint (400 cm), payload empty —
    // this is a photograph, not an encounter.
    FBreakerZoneSpec ProbeZone;
    ProbeZone.ZoneTag = FGameplayTag::RequestGameplayTag(TEXT("Zone.EffectProbe"), false);
    ProbeZone.RadiusCm = 400.0f;
    ProbeZone.Duration = Timing.DurationSeconds;
    FActorSpawnParameters ZoneParams;
    ZoneParams.ObjectFlags |= RF_Transient;
    ZoneParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if (ABreakerZoneActor* Zone = World->SpawnActor<ABreakerZoneActor>(
            ABreakerZoneActor::StaticClass(), FTransform(Frame.At(900.0f, 250.0f, 2.0f)), ZoneParams))
    {
        Zone->ConfigureZone(ProbeZone, nullptr);
        UE_LOG(LogTemp, Log, TEXT("[BreakerGym] effect probe: zone rim r=%.0f for %.1f s."),
            ProbeZone.RadiusCm, ProbeZone.Duration);
    }
}

bool ABreakerGameMode::IsBossAlive() const
{
    return IsValid(ActiveBoss) && !ActiveBoss->IsDeadEnemy();
}

void ABreakerGameMode::ResetBossEncounter()
{
    // O82 (amended): a solo death inside a boss encounter resets the
    // encounter rather than spending a budget — the dead boss progress is
    // the death's whole price in campaign.
    //
    // O121, AND WHY IT LIVES HERE TOO: the full reset is fair ONLY because
    // O18 puts a boss at twenty to forty-five seconds — losing that much
    // progress is a beat, not an evening. THE ENCOUNTER'S LENGTH IS WHAT
    // LICENSES THIS RULE. Anyone lengthening a boss fight past a few
    // minutes — more phases, a longer order cadence, a second health bar —
    // is obliged by O121 to replace this reset with a checkpoint, and this
    // comment exists because the TTK figure lives in a different file from
    // the death rule and nothing else connects them.
    if (!IsBossAlive()) return;
    UE_LOG(LogTemp, Display, TEXT("[BreakerGym] boss encounter RESET on player death (O82): the Field Marshal respawns whole."));
    ActiveBoss->Destroy();
    ActiveBoss = nullptr;
    SpawnBossTest();
}

void ABreakerGameMode::SpawnBossTest()
{
    UWorld* World = GetWorld();
    if (!World || !bFieldFrameSet) return;
    if (IsBossAlive())
    {
        UE_LOG(LogTemp, Display, TEXT("[BreakerGym] the Field Marshal is already alive; refusing a second."));
        return;
    }

    // The elite arena, which is the fourth combat pocket. Level-Design §5 puts
    // it at ArenaDistance with radius CombatPocketRadius (2000), and §5.1
    // notices that doubling that radius is EXACTLY Encounter-Design §3.3's
    // 4000 x 4000 boss room. So the arena is the right size by two independent
    // derivations — but only just: the boss's gallery offsets are ±1900 and the
    // pocket's broken wall arc sits at 1800-2200 cm from centre, so a gallery
    // can land inside a ruin segment. Checked and reported rather than assumed.
    const FVector ArenaCentre = Frame.At(ArenaDistance, 0.0f, 140.0f);
    if (CombatPocketRadius < BossArenaClearanceCm)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[BreakerGym] arena radius %.0f cm is under the boss's %.0f cm gallery reach; orders will point into geometry."),
            CombatPocketRadius, BossArenaClearanceCm);
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    // Facing back down the field, so a player arriving from the camp meets its
    // FRONT — which is the armoured side, and the whole fight is the decision
    // to stop being there.
    ABreakerBossEnemy* Boss = World->SpawnActor<ABreakerBossEnemy>(
        ABreakerBossEnemy::StaticClass(), ArenaCentre, (-Frame.Forward).Rotation(), Params);
    if (!Boss) return;

    Boss->ConfigureEncounter(ArenaCentre, 0.0f);
    Boss->SetAreaLevel(GymAreaLevel);
    // Rank Boss is authored in the class and must NOT be overwritten here;
    // SetAreaLevel rebuilds the chassis against whatever rank the archetype
    // set, which is exactly the one-source-of-truth rule O27 installed.
    UBreakerKillTelemetryComponent::AttachTo(Boss);
    Boss->OnBossDefeated.AddDynamic(this, &ABreakerGameMode::HandleBossDefeated);
    ActiveBoss = Boss;

    UE_LOG(LogTemp, Display,
        TEXT("[BreakerGym] FIELD MARSHAL spawned at the elite arena (%.0f cm forward), area level %d, %.0f health. Walk to it."),
        ArenaDistance, GymAreaLevel, Boss->GetMonsterMaxHealth());
}

void ABreakerGameMode::HandleBossDefeated()
{
    // O18 puts the boss band at 20-45s and Encounter-Design §3.2 records a
    // DIVERGENCE that is still open: the order cadence imposes a script floor
    // independent of health, so even a player who bursts a phase down waits on
    // the phases. Whether the composition lands inside the band is a
    // MEASUREMENT, and it is the boss TTK bucket that carries it — filed by the
    // kill-telemetry component from the boss's own rank, not from here.
    RefillPlayerAmmo();
    UE_LOG(LogTemp, Display, TEXT("[BreakerGym] FIELD MARSHAL down. Boss TTK sample recorded; F2 copies the report."));
}

float ABreakerGameMode::ResolveGroundZ(const APawn* Pawn, bool* bOutFoundFloor) const
{
    if (bOutFoundFloor) *bOutFoundFloor = false;
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
    const bool bFoundFloor = Lowest != TNumericLimits<float>::Max();
    if (bOutFoundFloor) *bOutFoundFloor = bFoundFloor;
    return bFoundFloor ? Lowest : Centre.Z - 88.0f;
}

// THE FIELD HAS NO FIXED WORLD POSITION, and that surprises people twice.
// Ground, forward and right are all derived from the possessed pawn, so the
// field is built relative to wherever the player happened to spawn and facing
// however they happened to face. Two PIE sessions started from different
// viewport camera positions produce the SAME field at different world
// coordinates. Consequences: comparing absolute coordinates between two
// sessions' screenshots or logs is meaningless, and every distance in this
// file is field-relative through Frame.At(), never a world vector.
void ABreakerGameMode::BuildFieldFrame(const APawn* Pawn)
{
    if (!Pawn) return;
    Frame.Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
    Frame.Right = Pawn->GetActorRightVector().GetSafeNormal2D();
    Frame.SpawnZ = Pawn->GetActorLocation().Z;
    bool bFoundFloor = false;
    const float GroundZ = ResolveGroundZ(Pawn, &bFoundFloor);
    Frame.bAuthoredFloor = bFoundFloor;
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
        // 7. Oblique over the ELITE ARENA. Added because the boss lives there
        //    and nothing pointed at it: the Field Marshal's galleries reach
        //    ±1900 cm against a 2000 cm pocket radius, and whether its orders
        //    point at open ground or into the pocket's ruin arc is a question
        //    only a picture answers.
        { Frame.At(ArenaDistance - CombatPocketRadius * 1.6f, -CombatPocketRadius * 1.3f, 2200.0f), FRotator(-24.0f, Frame.Forward.Rotation().Yaw + 34.0f, 0.0f) },
        // 8. THE GROUND ITSELF, at a grazing angle (owner: "a lot of the
        //    textures on the ground were tearing"). Z-fighting is invisible in a
        //    plan view and invisible from head height facing a wall; it needs a
        //    shallow angle across a large flat, and it gets worse with distance
        //    as depth precision falls off. This vantage stands over the jump-gap
        //    trench — whose floor was authored at the SAME top height as the
        //    apron under it — and looks out along 150 m of tint-patched apron,
        //    so both coplanar populations are in one frame.
        { Frame.At(EncounterPocketDistance + CombatPocketRadius + 2400.0f, 0.0f, 700.0f), FRotator(-11.0f, Frame.Forward.Rotation().Yaw, 0.0f) },
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
    // Counted by CLASS as well as by telemetry bucket. The summary line used to
    // say "melee N | ranged N", which was true and useless the moment four
    // archetypes existed: a Warden and a Skitter are both "melee" and a
    // Skirmisher and a Lattice are both "ranged", so the one line that proves
    // the gym spawned what it meant to could not tell them apart. A headless
    // smoke run reads this line to confirm the integration, so it has to name
    // every archetype it is asserting.
    int32 Melee = 0;
    int32 Ranged = 0;
    int32 Wardens = 0;
    int32 Skirmishers = 0;
    int32 Drudges = 0;
    int32 Lattices = 0;
    int32 Bosses = 0;
    int32 ModifierBearing = 0;
    int32 Elites = 0;
    int32 ModifierTotal = 0;
    TArray<FVector> AnchorLocations;
    TArray<FVector> RangedLocations;
    for (TActorIterator<ABreakerEnemy> It(const_cast<UWorld*>(World)); It; ++It)
    {
        if (It->IsRangedForTelemetry()) ++Ranged; else ++Melee;
        // Boss first: it SUBCLASSES the Warden (§3.1 "a Warden that commands
        // the other three archetypes"), so an unordered cast chain would count
        // the Field Marshal as a Warden and silently break the §5.3 cap check.
        if (It->IsA<ABreakerBossEnemy>()) { ++Bosses; AnchorLocations.Add(It->GetActorLocation()); }
        else if (It->IsA<ABreakerWardenEnemy>()) { ++Wardens; AnchorLocations.Add(It->GetActorLocation()); }
        else if (It->IsA<ABreakerSkirmisherEnemy>()) { ++Skirmishers; RangedLocations.Add(It->GetActorLocation()); }
        else if (It->IsA<ABreakerRangedEnemy>()) { ++Lattices; RangedLocations.Add(It->GetActorLocation()); }
        // Counted by name because it is otherwise indistinguishable from a
        // Skitter in this line, and "is the new archetype actually in the
        // world" is exactly the question a headless smoke run asks. Not a
        // Warden-class anchor: it has no frontal armour and no shield, so it
        // does not enter the 5.3 anchor cap.
        else if (It->IsA<ABreakerAlteredEnemy>()) { ++Drudges; }
        if (It->IsElite()) ++Elites;
        if (const UBreakerEnemyModifierComponent* Modifiers = It->GetModifierComponent();
            Modifiers && Modifiers->GetModifierCount() > 0)
        {
            ++ModifierBearing;
            ModifierTotal += Modifiers->GetModifierCount();
        }
    }
    int32 Targets = 0;
    for (TActorIterator<ABreakerTargetDummy> It(const_cast<UWorld*>(World)); It; ++It) ++Targets;
    UE_LOG(LogTemp, Display,
        TEXT("[BreakerGym] area level %d | melee %d | ranged %d | skitter/other %d | lattice %d | warden %d | skirmisher %d | drudge %d | boss %d | elite %d | modifier-bearing %d (%d modifiers) | target dummies %d"),
        GymAreaLevel, Melee, Ranged,
        Melee + Ranged - Lattices - Wardens - Skirmishers - Bosses - Drudges,
        Lattices, Wardens, Skirmishers, Drudges, Bosses, Elites, ModifierBearing, ModifierTotal, Targets);
    // The §5.3 caps, asserted rather than assumed. They are the difference
    // between "dense" and "unplayable", and this check has already earned its
    // keep once: it caught two Skirmishers standing alongside two Lattices in
    // the encounter, which is four converging projectile sources against a cap
    // of three.
    //
    // Counted PER ENCOUNTER, not per world. The caps are about what is in one
    // fight — the field holds a standing encounter at 8500 cm and an arena at
    // 17000, and a Warden in one plus the Field Marshal in the other is two
    // separate fights, not an illegal one. Proximity is the only definition of
    // "one fight" available here, and one combat pocket's diameter is the
    // honest radius for it.
    const float EncounterRadius = CombatPocketRadius * 2.0f;
    auto WarnOnCrowding = [&](const TArray<FVector>& Locations, int32 Cap, const TCHAR* What, const TCHAR* Reason)
    {
        for (const FVector& Centre : Locations)
        {
            int32 Nearby = 0;
            for (const FVector& Other : Locations)
            {
                if (FVector::DistSquared2D(Centre, Other) <= FMath::Square(EncounterRadius)) ++Nearby;
            }
            if (Nearby > Cap)
            {
                UE_LOG(LogTemp, Warning, TEXT("[BreakerGym] %d %s within one encounter; Encounter-Design 5.3 caps them at %d — %s"),
                    Nearby, What, Cap, Reason);
                return;
            }
        }
    };
    WarnOnCrowding(AnchorLocations, 1, TEXT("Warden-class anchors"),
        TEXT("overlapping frontal-armour anchors create unsolvable geometry."));
    WarnOnCrowding(RangedLocations, 3, TEXT("ranged sources"),
        TEXT("four converging projectile sources removes all safe ground."));

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
    // A single upward quad with NO side walls. Ground tinting has to be laid
    // above the apron to avoid coplanarity, and a lifted CUBE pays for that
    // with a vertical lip whose shaded face is sub-pixel at field distances and
    // aliases into a dashed dark line tracing every patch outline — which is
    // most of what the ground-tearing report was looking at. A plane has no lip
    // to alias.
    const TCHAR* ShapePlane    = TEXT("/Engine/BasicShapes/Plane.Plane");

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
    // Top at GroundOverlayLift, NOT at 0. It was 0, which is exactly the apron's
    // top height, and this slab sits ON the apron — two coplanar surfaces over
    // the whole trench, i.e. guaranteed z-fighting (owner: "a lot of the
    // textures on the ground were tearing"). The lift is centimetres: the drop
    // off the take-off platform goes 220 -> 214 cm, which changes no jump and
    // no landing band.
    SpawnFieldSlab(World, Frame, TrenchFwd, TrenchFwd + SwiftThreeJumpGap,
        -PlatformWidth * 2.6f, PlatformWidth * 2.6f, GroundOverlayLift, 30.0f, PaletteStone, TEXT("Runtime_JumpGap"));
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

    // Kess and the Quartermaster no longer spawn here (owner ruling
    // 2026-08-16, A10 -> O48: the Anchor hub is their ONLY home —
    // BreakerHubBuilder.cpp spawns them, this camp does not). The camp keeps
    // its physical props (forge, crate, supply trigger) as set dressing.
    // Consequence, deliberate: quest offer and turn-in now require travelling
    // to the Anchor — including from a PIE drop-in on the template map, where
    // there is no vendor at all until you take the travel point to the hub.

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
            UBreakerKillTelemetryComponent::AttachTo(Enemy);
        }
    }

    // NON-ELITE MODIFIER CARRIERS (O27's kill-bucket producer). Until this,
    // modifiers only ever landed on the elite below, and GrantModifiers
    // restores the authored rank afterwards — correct for an elite, since
    // ModifierBearing (x2.5) would otherwise DEMOTE it from Elite (x3.0) — so
    // rank ModifierBearing never existed at kill time. Playtest/
    // BreakerKillBuckets.h calls that bucket "the one number that says
    // whether [O27] worked"; it was structurally empty. These plain trash
    // bodies KEEP the promotion instead (O9 keeps Rank and Modifiers
    // separate, so this does not contradict the elite's own tell).
    const float CarrierLateralOffsets[] = { -250.0f, 250.0f };
    for (int32 Index = 0; Index < GymModifierCarrierCount; ++Index)
    {
        const FVector SpawnLocation = Frame.At(
            EncounterPocketDistance - CombatPocketRadius * 0.2f,
            CarrierLateralOffsets[Index % 2], 120.0f);
        FActorSpawnParameters CarrierParams;
        CarrierParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        if (ABreakerEnemy* Carrier = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, CarrierParams))
        {
            Carrier->ConfigureEncounter(SpawnLocation, 2.1f + Index * 0.6f);
            Carrier->SetAreaLevel(GymAreaLevel);
            // Offset well clear of the elite's own ModifierSeedBase draw so
            // the two rolls never share a stream position.
            GrantModifierCarrier(Carrier, ModifierSeedBase + 500 + Index * 97);
            UBreakerKillTelemetryComponent::AttachTo(Carrier);
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
        UBreakerKillTelemetryComponent::AttachTo(Elite);
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
            UBreakerKillTelemetryComponent::AttachTo(Ranged);
        }
    }

    // ONE Warden, front and centre (Encounter-Design §5.3: live Wardens per
    // player = 1, "frontal-armour anchors overlapping create unsolvable
    // geometry"). It stands in FRONT of the pack rather than behind it, which
    // is the point of the archetype: §2.4's third axis is "Wardens punish
    // approaching from the front", so the player meets it first and has to
    // decide to go around something instead of through it.
    const FVector WardenLocation = Frame.At(EncounterPocketDistance - CombatPocketRadius * 0.85f, 0.0f, 120.0f);
    FActorSpawnParameters WardenParams;
    WardenParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    if (ABreakerWardenEnemy* Warden = World->SpawnActor<ABreakerWardenEnemy>(
        ABreakerWardenEnemy::StaticClass(), WardenLocation, FRotator::ZeroRotator, WardenParams))
    {
        Warden->ConfigureEncounter(WardenLocation, 1.4f);
        Warden->SetAreaLevel(GymAreaLevel);
        UBreakerKillTelemetryComponent::AttachTo(Warden);
    }

    // THE SEVERED DRUDGE (O40c reachability). ABreakerAlteredEnemy shipped and
    // was tested and was spawned by NOTHING, which is the exact defect O40(c)
    // exists to prevent: content in the codebase that no player can reach.
    //
    // ENCOUNTER-DESIGN GATE, stated rather than skirted: the document gates all
    // Altered content behind the Act II turn beat. The gym is a playtest
    // instrument, not campaign content, so this is legitimate — and the Drudge
    // must not be added to anything that reads as campaign progression until
    // that beat exists.
    //
    // Placed on the pocket's OPEN side, off the corridor axis, and then moved to
    // whatever ground near there carries no hard cover: its weak point is a
    // dorsal ridge at 129 cm rather than a head, so the answer to it is a circle,
    // and a 2.0x-health body with its back to a slab has no answer at all. It is
    // also a MELEE body against 5.3's live-enemy ceiling — one, not two, keeps
    // the standing encounter at eleven live against a cap of twelve.
    for (int32 Index = 0; Index < GymDrudgeCount; ++Index)
    {
        const FVector DrudgeAround = Frame.At(EncounterPocketDistance - CombatPocketRadius * 0.25f,
            (Index % 2 == 0 ? -1.0f : 1.0f) * (CombatPocketRadius * 0.45f), 120.0f);
        SpawnDrudge(DrudgeAround, 2.4f + Index * 0.8f, GymAreaLevel);
    }

    // ONE Skirmisher, and its placement is the whole point. It goes AT the
    // pocket's cover ring, not at an arbitrary bearing: the pocket's four cover
    // blocks sit on a CoverPitchMax ring around the pocket centre, and a
    // Skirmisher that starts beside one of them is behind cover on frame one.
    // Spawned in the open it is a plain shooter with a longer telegraph than a
    // Lattice, which is strictly worse than a Lattice and teaches nothing.
    //
    // ONE, not two, and the cap check in LogGymSummary is what caught it: two
    // Skirmishers alongside the two flanking Lattices is FOUR converging
    // projectile sources, and §5.3 holds that at three at any party size
    // because "four converging projectile sources removes all safe ground —
    // this is the single most dangerous scaling knob". Wave mode introduces
    // more of them, inside the same budget solver that enforces the same cap.
    const FVector PocketCentre = Frame.At(EncounterPocketDistance, 0.0f, 0.0f);
    const FVector PlayerApproach = Frame.At(EncounterPocketDistance - CombatPocketRadius * 2.0f, 0.0f, 0.0f);
    for (int32 Index = 0; Index < 1; ++Index)
    {
        // Two different bearings off the pocket centre so they resolve to two
        // different cover blocks rather than crowding one.
        const FVector Bearing = Frame.Forward.RotateAngleAxis(Index == 0 ? 55.0f : -55.0f, FVector::UpVector);
        if (ABreakerSkirmisherEnemy* Skirmisher = SpawnSkirmisherNearCover(
            PocketCentre + Bearing * (CoverPitchMax * 0.5f), PlayerApproach, 0.6f + Index * 1.3f))
        {
            Skirmisher->SetAreaLevel(GymAreaLevel);
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

void ABreakerGameMode::GrantModifierCarrier(ABreakerEnemy* Enemy, int32 Seed) const
{
    if (!bGrantModifiers || !Enemy) return;

    // No capture-and-restore: ConfigureWithModifiers's unconditional
    // promotion to rank ModifierBearing is exactly what a carrier is for —
    // see Playtest/BreakerKillBuckets.h. If the roll grants zero (a legal
    // outcome of RollAndApplyModifiers for a pathological family/params
    // combination), the body simply stays rank Trash; nothing else to do.
    Enemy->ConfigureWithModifiers(Seed);
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

void ABreakerGameMode::SpawnCombatPocket(float Fwd, float Rgt, FRandomStream& Stream, bool bRimRuins)
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
    // bRimRuins false is the ELITE ARENA. The arc sits 1800-2200 cm from centre
    // and the Field Marshal's gallery offsets are +/-1900 with alcoves at
    // +/-1700, so in the arena this arc is geometry the boss gives orders into.
    // SpawnBossTest has warned about exactly that since it was written; leaving
    // the arc out is the fix rather than the warning.
    for (int32 Segment = 0; bRimRuins && Segment < 5; ++Segment)
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

    // THE POCKET'S COVER MOVED OUT OF HERE. It used to be four blocks and a
    // pillar authored inline, registered as bare positions, and invisible to
    // anything that wanted to reason about the field: nothing could tell the
    // 500 cm pillar from the 110 cm blocks, and nothing outside the pocket had
    // any cover at all. Both are now the cover field's job
    // (Game/BreakerCoverRegistry.h, SpawnCoverField below), which places the
    // same cluster at every pocket centre plus an outer ring, classifies each
    // piece, and can be measured. What is left in this function is dressing:
    // the broken rim arc, the overgrowth and the fallen prop.

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
    // THE FIFTH SLAB, only when the courtyard the four above abut DOES NOT
    // EXIST. The shipped Lvl_Gym is an empty asset — no authored floor at all
    // — so the ground probe found nothing and the rectangle-around-the-shell
    // authoring left a 4000 x 4000 hole exactly under the arriving pawn
    // ("theres no floor to the gym"). In Lvl_FirstPerson the probe hits the
    // template Floor, this slab is skipped, and the abutting-never-overlapping
    // rule (no coplanar z-fighting) is preserved untouched.
    if (!Frame.bAuthoredFloor)
    {
        SpawnFieldSlab(World, Frame, -Shell, Shell, -Shell, Shell, 0.0f, 40.0f, PaletteEarth, TEXT("Runtime_FieldApron"));
        UE_LOG(LogTemp, Display, TEXT("[BreakerGym] no authored floor under the shell — centre apron slab spawned."));
    }

    // Tint patches: non-colliding flat plates that break 250 x 220 m of one
    // colour. Purely so the eye has something to judge speed against — a
    // featureless plane is a large part of why a big field can still read as
    // nothing (O24 dressing, no gameplay meaning).
    // Deliberately SMALL and numerous. The first version used 900-2600 cm
    // plates and the plan view read as farmland — a patch the size of a combat
    // pocket is a landmark, not a texture, and it lies about the scale of the
    // thing next to it. Scrub-sized plates give the eye optical flow to judge
    // speed against without ever being mistaken for geometry.
    //
    // TWO THINGS HERE ARE BUG FIXES, not dressing (owner: "a lot of the
    // textures on the ground were tearing"). Both were the same mistake in two
    // forms — a flat plate laid on a flat plane with nothing separating them:
    //
    //  1. The patches were placed by pure rejection-free random sampling, so
    //     they OVERLAPPED each other, and every patch is authored at the same
    //     height. Two overlapping plates whose top faces are at exactly the
    //     same z are coplanar, and coplanar is z-fighting by construction: the
    //     depth test has no winner and the pair stipples. At ~18% area coverage
    //     over 200 plates that is dozens of overlapping pairs scattered across
    //     the whole field, which is exactly what "a lot of" describes. The
    //     footprints are now tracked and an overlapping placement is REJECTED,
    //     so no two patches ever share a surface. Rejection also removes the
    //     double-tinted blotches, which is a second, smaller win.
    //  2. The patches were 4 cm-thick CUBES that CAST SHADOWS. At 150-200 m
    //     both the shadow of that lip and the lip's own shaded side face are
    //     sub-pixel, and both alias into a stippled dashed line tracing the
    //     patch outline — the dark dotted seams along every patch edge in the
    //     before-capture. Killing the shadow removed most of it and left the
    //     side face still drawing a fainter one, which is measured, not
    //     assumed: it is visible in the intermediate capture. So the patches are
    //     now PLANES — one upward quad, no lip to shade and none to alias —
    //     lifted clear of the apron, casting nothing.
    //
    // The attempt count is raised because rejection now throws placements away;
    // it is attempts, not patches, and the field settles at rather fewer.
    struct FPatchFootprint { float MinFwd, MaxFwd, MinRgt, MaxRgt; };
    TArray<FPatchFootprint> Placed;
    Placed.Reserve(FieldPatchAttempts);
    for (int32 Patch = 0; Patch < FieldPatchAttempts; ++Patch)
    {
        const float Fwd = Stream.FRandRange(Back, Front);
        const float Rgt = Stream.FRandRange(-Side, Side);
        if (FMath::Abs(Fwd) < Shell && FMath::Abs(Rgt) < Shell) continue;
        const float SizeX = Stream.FRandRange(320.0f, 1100.0f);
        const float SizeY = Stream.FRandRange(320.0f, 1100.0f);
        const float Yaw = Stream.FRandRange(0.0f, 360.0f);
        // Axis-aligned bound of the ROTATED plate, so the rejection is a true
        // separation test rather than one that passes on a corner overlap.
        const float CosYaw = FMath::Abs(FMath::Cos(FMath::DegreesToRadians(Yaw)));
        const float SinYaw = FMath::Abs(FMath::Sin(FMath::DegreesToRadians(Yaw)));
        const float HalfFwd = 0.5f * (SizeX * CosYaw + SizeY * SinYaw);
        const float HalfRgt = 0.5f * (SizeX * SinYaw + SizeY * CosYaw);
        const FPatchFootprint Footprint{ Fwd - HalfFwd, Fwd + HalfFwd, Rgt - HalfRgt, Rgt + HalfRgt };
        bool bOverlaps = false;
        for (const FPatchFootprint& Other : Placed)
        {
            if (Footprint.MinFwd < Other.MaxFwd && Footprint.MaxFwd > Other.MinFwd &&
                Footprint.MinRgt < Other.MaxRgt && Footprint.MaxRgt > Other.MinRgt)
            {
                bOverlaps = true;
                break;
            }
        }
        if (bOverlaps) continue;
        Placed.Add(Footprint);
        AStaticMeshActor* PatchActor = SpawnShape(World, ShapePlane, Frame.At(Fwd, Rgt, GroundOverlayLift * 0.5f),
            FVector(SizeX / 100.0f, SizeY / 100.0f, 1.0f),
            FRotator(0.0f, Yaw, 0.0f),
            FMath::Lerp(PaletteEarth, Stream.FRand() < 0.3f ? PaletteDryGrass : PaletteMoss, Stream.FRandRange(0.35f, 1.0f)),
            false, TEXT("Runtime_FieldPatch"));
        if (PatchActor)
        {
            PatchActor->GetStaticMeshComponent()->SetCastShadow(false);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("[BreakerGym] tint patches: %d placed from %d attempts (overlaps rejected; a coplanar pair is z-fighting by construction)"),
        Placed.Num(), FieldPatchAttempts);

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
    // bRimRuins false: see SpawnCombatPocket. The arena's cover is authored to
    // Encounter-Design 3.3 by the cover field instead.
    SpawnCombatPocket(ArenaDistance, 0.0f, Stream, false);

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
    // The lane's one piece of hard cover — RangedSightlineDepth of clear ground
    // behind it, the minimum a LATTICE needs to use its whole 900-1900 band
    // instead of backing into a kerb — is now placed by the cover field, which
    // is also what knows to keep every other piece off this lane.

    // --- 5. THE COVER FIELD ------------------------------------------------
    // Last, because it measures the band it is filling and the exclusions it
    // measures against are the stations built above.
    SpawnCoverField();
}

FBreakerCoverFieldParams ABreakerGameMode::MakeCoverFieldParams() const
{
    // EVERY grammar number is transported, never re-authored. Level-Design 3 is
    // the single source of truth for CoverPitchMax, CombatPocketRadius,
    // DashCorridorWidth, RangedSightlineDepth, SafeZoneRadius and ArenaDistance,
    // and the cover field is derived from them — so retuning the grammar
    // retunes the cover with it instead of leaving two numbers to drift apart.
    FBreakerCoverFieldParams Params;
    Params.CoverPitchMaxCm = CoverPitchMax;
    Params.DashCorridorWidthCm = DashCorridorWidth;
    Params.CombatPocketRadiusCm = CombatPocketRadius;
    Params.RangedSightlineDepthCm = RangedSightlineDepth;
    Params.SafeZoneRadiusCm = SafeZoneRadius;
    Params.ArenaDistanceCm = ArenaDistance;

    Params.ClusterPitchCm = CoverClusterPitch;
    Params.ClusterRingRadiusCm = CoverClusterRingRadius;
    Params.ChestHeightCm = CoverChestHeight;
    Params.FullHeightCm = CoverFullHeight;
    Params.PocketPillarHeightCm = FMath::Max(CoverFullHeight, 500.0f);
    Params.PocketInnerRingRadiusCm = CoverPitchMax * 0.5f;
    Params.WidestEnemyBodyCm = 120.0f;   // the SEVERED DRUDGE's overridden capsule

    // The contested band: from the target range's firing line to past the elite
    // arena, stopping short of the sniper lane and the wall-ride corridor.
    Params.BandNearCm = RangeFiringLineDistance - 600.0f;
    Params.BandFarCm = ArenaDistance + CombatPocketRadius;
    Params.BandHalfWidthCm = CoverBandHalfWidth;

    // The instrument corridor: the firing line, its four dummies (laterals
    // -850 .. +350) and the player's line in to the standing encounter.
    Params.CorridorNearCm = RangeFiringLineDistance - 600.0f;
    Params.CorridorFarCm = EncounterPocketDistance + CombatPocketRadius;
    Params.CorridorShoulderPitchCm = CoverPitchMax;

    // THE POCKETS. SpawnExpandedField builds four; the first is the standing
    // encounter and sits ON the corridor, so it gets flank line breaks rather
    // than a cluster, and the fourth IS the elite arena, which is authored to
    // Encounter-Design 3.3 separately.
    Params.CorridorPocketCentres.Add(FVector2D(EncounterPocketDistance, 0.0f));
    Params.PocketCentres.Add(FVector2D(EncounterPocketDistance + DashRefreshDistance, FieldHalfExtent * 0.55f));
    Params.PocketCentres.Add(FVector2D(RangeFiringLineDistance + DashRefreshDistance, -FieldHalfExtent * 0.55f));
    Params.EncounterFlankOffsetCm = FMath::Min(CoverPitchMax - 100.0f, 1600.0f);

    // THE JUMP-GAP RUN, taken from SpawnJumpGapRun's own arithmetic rather than
    // restated: trench at EncounterPocketDistance + CombatPocketRadius + 2200,
    // platforms DashCorridorWidth wide with the take-off 1600 cm behind it and
    // the trench floor 2.6 platform widths to each side.
    const float TrenchFwd = EncounterPocketDistance + CombatPocketRadius + 2200.0f;
    Params.JumpRunNearCm = TrenchFwd - 2000.0f;
    // +1200 rather than the run's full 1600 cm landing depth plus its ramp: the
    // elite arena's marker ring starts at 15000 and the third jump lane's
    // landing ends at 16400, so the two stations PHYSICALLY OVERLAP in the field
    // this pass inherited. A box drawn to the run's true far edge swallows the
    // arena's own §3.3 pillars. 16000 covers the trench, the take-offs and the
    // first two landings, and the third landing is protected instead by the
    // arena exclusion, which reaches 3904 cm from the arena centre and therefore
    // covers everything from 13096 forward. Reported to the owner as a station
    // collision rather than papered over.
    Params.JumpRunFarCm = TrenchFwd + SwiftThreeJumpGap + 1200.0f;
    Params.JumpRunHalfWidthCm = DashCorridorWidth * 2.6f;

    // THE MOVEMENT LANES, likewise taken from the spawners that build them.
    Params.SniperLaneRightCm = -FieldHalfExtent * 0.62f;
    Params.SniperLaneHalfWidthCm = DashCorridorWidth * 0.5f;
    Params.WallLaneRightCm = FieldHalfExtent * 0.62f;
    Params.WallLaneHalfWidthCm = WallRideCorridorWidth * 0.5f + 125.0f;

    // The lane's own hard cover, in the position the shipped field gave it.
    Params.LaneCoverForwardCm = RangeFiringLineDistance - 1500.0f + RangedSightlineDepth;
    Params.LaneCoverRightCm = Params.SniperLaneRightCm + 500.0f;

    Params.Seed = ModifierSeedBase;
    return Params;
}

void ABreakerGameMode::SpawnCoverField()
{
    UWorld* World = GetWorld();
    if (!World || !bFieldFrameSet || !bBuildCoverField) return;

    const FBreakerCoverFieldParams Params = MakeCoverFieldParams();
    const TArray<FBreakerCoverPiece> Pieces = UBreakerCoverLayoutLibrary::BuildCoverField(Params);

    for (const FBreakerCoverPiece& Piece : Pieces)
    {
        // Sunk 5 cm so the bottom face is never coplanar with the apron. The
        // face is hidden either way, but the field has already shipped one
        // z-fighting report and the fix costs nothing. A chest-high piece is
        // still 115 cm proud, under MantleStepHeight 145, so it stays climbable.
        const float CentreZ = Piece.HeightCm * 0.5f - 5.0f;
        const FVector Location = Frame.At(Piece.Forward, Piece.Right, CentreZ);
        SpawnShape(World, ShapeCube, Location,
            FVector(Piece.HalfLengthCm * 2.0f / 100.0f, Piece.HalfDepthCm * 2.0f / 100.0f, Piece.HeightCm / 100.0f),
            FRotator(0.0f, Frame.Forward.Rotation().Yaw + Piece.YawDegrees, 0.0f),
            Piece.Class == EBreakerCoverClass::FullHeight ? PaletteConcrete : PaletteStone,
            true, Piece.Class == EBreakerCoverClass::FullHeight ? TEXT("Runtime_CoverFull") : TEXT("Runtime_CoverChest"));
        RegisterCoverAnchor(Location, Piece.Class, Piece.HeightCm);
    }

    UE_LOG(LogTemp, Display, TEXT("[BreakerGym] %s"),
        *UBreakerCoverLayoutLibrary::DescribeCoverField(Pieces, Params));
    FString Reason;
    if (!UBreakerCoverLayoutLibrary::IsLayoutLegal(Pieces, Params, Reason))
    {
        // Loud, never silent. Every rule named here has a reason written beside
        // it in Level-Design 3/4 or Encounter-Design 3.3, and a field that
        // breaks one is a field that measures the wrong thing.
        UE_LOG(LogTemp, Warning, TEXT("[BreakerGym] COVER FIELD IS ILLEGAL: %s"), *Reason);
    }
}

FVector ABreakerGameMode::FindFlankableGround(const FVector& Around, float SearchRadius) const
{
    // A SEVERED DRUDGE is answered by getting BEHIND it: its weak point is a
    // dorsal ridge at 129 cm, not a head, so the whole archetype is a circle.
    // Spawning one with its back against a 400 cm slab deletes the answer and
    // leaves a 2.0x-health body with no counterplay, which is the opposite of
    // what this archetype is for. This walks a ring of candidates and returns
    // the first whose flanking circle is clear of hard cover.
    FBreakerCoverAnchor Anchor;
    if (!CoverRegistry.FindNearest(Around, DrudgeFlankClearanceCm, Anchor)) return Around;
    for (int32 Step = 1; Step <= 12; ++Step)
    {
        // A spiral rather than a ring: bearing turns by the golden angle and the
        // radius grows, so twelve candidates cover the neighbourhood evenly
        // instead of all landing on one arc.
        const float Bearing = Step * 137.5f;
        const float Radius = DrudgeFlankClearanceCm * (0.6f + 0.5f * Step);
        if (Radius > SearchRadius) break;
        const FVector Candidate = Around
            + FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(Bearing, FVector::UpVector) * Radius;
        if (!CoverRegistry.FindNearest(Candidate, DrudgeFlankClearanceCm, Anchor)) return Candidate;
    }
    // No clear circle anywhere nearby. Honest answer: put it where it was asked
    // for and say so, rather than silently walking it across the field.
    UE_LOG(LogTemp, Display,
        TEXT("[BreakerGym] no cover-free circle of %.0f cm within %.0f cm of (%.0f, %.0f); the Drudge there will be harder to flank."),
        DrudgeFlankClearanceCm, SearchRadius, Around.X, Around.Y);
    return Around;
}

ABreakerAlteredEnemy* ABreakerGameMode::SpawnDrudge(const FVector& Around, float PatrolPhase, int32 AreaLevel)
{
    UWorld* World = GetWorld();
    if (!World || !bSpawnDrudges) return nullptr;

    // Z is the CALLER'S. The standing encounter builds its point on the field
    // frame; wave mode builds its arena around wherever the player is standing,
    // which may not be the frame's ground plane at all. Forcing the frame's Z
    // here would drop a wave Drudge through a platform the playtester is fighting
    // on. Enemies ground-snap every tick regardless.
    const FVector SpawnLocation = FindFlankableGround(Around, CombatPocketRadius);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    ABreakerAlteredEnemy* Drudge = World->SpawnActor<ABreakerAlteredEnemy>(
        ABreakerAlteredEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, Params);
    if (!Drudge) return nullptr;
    Drudge->ConfigureEncounter(SpawnLocation, PatrolPhase);
    Drudge->SetAreaLevel(AreaLevel);
    UBreakerKillTelemetryComponent::AttachTo(Drudge);
    return Drudge;
}

void ABreakerGameMode::RegisterCoverAnchor(const FVector& WorldLocation, EBreakerCoverClass Class, float HeightCm)
{
    CoverRegistry.Add(WorldLocation, Class, HeightCm);
}

bool ABreakerGameMode::FindCoverAnchorNear(const FVector& Around, float MaxDistance, FVector& OutAnchor) const
{
    // The 2D-ness and the "no cover here is a real answer" contract both live in
    // FBreakerCoverRegistry now, which is where they can be tested.
    FBreakerCoverAnchor Anchor;
    if (!CoverRegistry.FindNearest(Around, MaxDistance, Anchor)) return false;
    OutAnchor = Anchor.Location;
    return true;
}

ABreakerSkirmisherEnemy* ABreakerGameMode::SpawnSkirmisherNearCover(const FVector& Around,
    const FVector& ThreatLocation, float PatrolPhase)
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;

    // Stand it just BEHIND the cover relative to the threat. Its opening state
    // is Relocating, so the first thing it does is look for a point whose line
    // from the threat is blocked; starting on the blocked side means it finds
    // one on frame one instead of walking across open ground to get there.
    // FULL-HEIGHT FIRST. A 120 cm block does not break a line of sight — the
    // Skirmisher's own cover search traces from the threat and the trace is
    // real — so ducking behind chest-high cover leaves it visible and it is a
    // plain shooter again. Chest-high is the fallback rather than the answer,
    // and the log below says which one it got.
    FVector Anchor = Around;
    FBreakerCoverAnchor Chosen;
    bool bHasCover = CoverRegistry.FindNearestOfClass(Around, CoverPitchMax, EBreakerCoverClass::FullHeight, Chosen);
    if (!bHasCover) bHasCover = CoverRegistry.FindNearest(Around, CoverPitchMax, Chosen);
    if (bHasCover) Anchor = Chosen.Location;
    FVector SpawnLocation = Anchor;
    if (bHasCover)
    {
        const FVector AwayFromThreat = (Anchor - ThreatLocation).GetSafeNormal2D();
        // 260 cm: clear of the cover block's own footprint (the pocket blocks
        // are up to 260 cm on their long axis) and well inside the 1400 cm
        // search radius, so every candidate ring it generates still contains
        // this piece.
        SpawnLocation = Anchor + AwayFromThreat * 260.0f;
    }
    SpawnLocation.Z = Frame.Ground.Z + 120.0f;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    ABreakerSkirmisherEnemy* Skirmisher = World->SpawnActor<ABreakerSkirmisherEnemy>(
        ABreakerSkirmisherEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, Params);
    if (!Skirmisher) return nullptr;
    Skirmisher->ConfigureEncounter(SpawnLocation, PatrolPhase);
    UBreakerKillTelemetryComponent::AttachTo(Skirmisher);
    if (!bHasCover)
    {
        // Loud, because a Skirmisher with nothing to hide behind is a plain
        // shooter and the whole archetype has quietly stopped existing. That is
        // exactly the failure its own class note warns about.
        UE_LOG(LogTemp, Warning,
            TEXT("[BreakerGym] Skirmisher spawned with NO cover anchor within %.0f cm of (%.0f, %.0f) — it will degrade to an open-ground shooter."),
            CoverPitchMax, Around.X, Around.Y);
    }
    else if (Chosen.Class != EBreakerCoverClass::FullHeight)
    {
        // Not a warning: chest-high cover is legitimate ground for it, and the
        // instrument corridor carries nothing else on purpose. It is recorded
        // because a run whose Skirmishers all found chest-high cover is a
        // different measurement from one where they found line breaks.
        UE_LOG(LogTemp, Display,
            TEXT("[BreakerGym] Skirmisher anchored on CHEST-HIGH cover (%.0f cm) at (%.0f, %.0f); no line break within %.0f cm."),
            Chosen.HeightCm, Chosen.Location.X, Chosen.Location.Y, CoverPitchMax);
    }
    return Skirmisher;
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

    // THE WAVE IS SOLVED, NOT RAMPED. What was here was `4 + wave * 3` capped
    // at 24, an elite every third wave and `wave/2` Lattices: no budget, no
    // archetype costs, no rest waves, no boss wave, no variety rule, and 24
    // live enemies against Encounter-Design §5.3's ceiling of TWELVE.
    // UBreakerWaveBudgetLibrary is §4.2's arithmetic as pure world-free maths,
    // so what a wave IS can be asserted by automation, and all this function
    // does is place the answer in the world.
    const FBreakerWaveComposition Composition =
        UBreakerWaveBudgetLibrary::SolveWave(CurrentWave, 1, WaveBudget);
    FString IllegalReason;
    if (!UBreakerWaveBudgetLibrary::IsCompositionLegal(Composition, 1, WaveBudget, IllegalReason))
    {
        // Loud, never silent, and never trimmed: a spawner that quietly drops
        // an enemy makes the instrument report a wave that did not happen.
        UE_LOG(LogTemp, Warning, TEXT("[BreakerGym] wave %d composition is ILLEGAL: %s"), CurrentWave, *IllegalReason);
    }
    const int32 AreaLevel = GetAreaLevelForWave(CurrentWave);
    UE_LOG(LogTemp, Display, TEXT("[BreakerGym] %s | area level %d"),
        *UBreakerWaveBudgetLibrary::DescribeComposition(Composition), AreaLevel);

    // --- The boss wave (§4.2, wave 12) -------------------------------------
    // The Field Marshal and nothing else. It deploys its own adds and respawns
    // its own gallery Lattices, and §5.3's density ceiling is enforced at that
    // SOURCE — a wave budget spent alongside it would blow the cap from two
    // directions at once and neither would know about the other.
    if (Composition.bBoss)
    {
        SpawnBossTest();
        if (IsValid(ActiveBoss))
        {
            ActiveBoss->ConfigureWave(AreaLevel);
            WaveEnemies.Add(ActiveBoss);
        }
        return;
    }

    // --- Melee, including the elite promotions ------------------------------
    // An elite is a PROMOTED body, not an extra one, which is what keeps the
    // density ceiling honest: the solver counts elites inside Skitters.
    // THE DRUDGE IN WAVE MODE, as a SUBSTITUTION. BreakerWaveBudget.h has no
    // Drudge row and this lane may not add one, so a Drudge is rendered in place
    // of a melee body the solver already paid for — exactly the precedent the
    // solver itself sets for elites and modifier carriers, which are promoted
    // Skitters folded into Composition.Skitters rather than extra bodies. That
    // keeps 5.3's density ceiling honest without touching the solver.
    //
    // WHAT THE SOLVER WOULD NEED, if the owner wants a Drudge to be a real
    // archetype rather than a re-skin — reported, not made:
    //   FBreakerWaveBudgetParams: int32 DrudgeCost = 2;  int32 DrudgeFromWave = 3;
    //   FBreakerWaveComposition:  int32 Drudges = 0;  folded into TotalEnemies()
    //                             and counted as melee, not as a ranged source.
    // Cost 2 rather than the Skitter's 1: 2.0x health and no lunge is roughly
    // two Skitters' worth of time-to-kill and none of a Skitter's pressure.
    //
    // Substituted from the END of the melee list so the elite and carrier
    // promotions, which take the low indices, are never turned into Drudges —
    // the Drudge overrides its own capsule and chassis and a promoted one would
    // be reading two archetypes at once.
    int32 WaveDrudges = 0;
    if (bSpawnDrudges && CurrentWave >= DrudgeFromWave)
    {
        WaveDrudges = FMath::Clamp(CurrentWave / FMath::Max(1, DrudgeWaveDivisor), 0, MaximumDrudgesPerWave);
        WaveDrudges = FMath::Min(WaveDrudges,
            FMath::Max(0, Composition.Skitters - Composition.Elites - Composition.ModifierCarriers));
    }
    const int32 FirstDrudgeIndex = Composition.Skitters - WaveDrudges;

    for (int32 Index = 0; Index < Composition.Skitters; ++Index)
    {
        const int32 Pack = Index / 4;
        const float PackAngle = 360.0f * Pack / FMath::Max(1, (Composition.Skitters + 3) / 4);
        // Packs sit on the pocket rim rather than at a flat 1100 cm, so the
        // ring the player circles is the same radius everywhere in the field.
        const FVector PackCenter = ArenaCenter + FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(PackAngle, FVector::UpVector) * (CombatPocketRadius * 0.55f);
        const FVector SpawnLocation = PackCenter + FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(Index * 90.0f, FVector::UpVector) * 160.0f;
        if (WaveDrudges > 0 && Index >= FirstDrudgeIndex)
        {
            if (ABreakerAlteredEnemy* Drudge = SpawnDrudge(SpawnLocation, Index * 1.3f, AreaLevel))
            {
                Drudge->ConfigureWave(AreaLevel);
                SetEnemyDropsLoot(Drudge, Composition.bDropsLoot);
                WaveEnemies.Add(Drudge);
            }
            continue;
        }
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        ABreakerEnemy* Enemy = GetWorld()->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, Params);
        if (!Enemy) continue;
        Enemy->ConfigureEncounter(SpawnLocation, Index * 1.3f);
        Enemy->ConfigureWave(AreaLevel);
        if (Index < Composition.Elites)
        {
            Enemy->ConfigureElite();
            // Seeded on the WAVE and the index, so wave 8 meets the same
            // Champion every run and a TTK sample taken across two sessions
            // compares. The solver decided HOW MANY modifiers it could afford;
            // the roll decides which, subject to §1.3's composition rules.
            GrantModifiers(Enemy, ModifierSeedBase + CurrentWave * 7919 + Index);
        }
        else if (Index < Composition.Elites + Composition.ModifierCarriers)
        {
            // Non-elite modifier carriers (O27's kill-bucket producer): KEEP
            // rank ModifierBearing rather than restoring an authored rank, the
            // same distinction GrantModifierCarrier draws against GrantModifiers
            // above. Seeded the same way, offset past the elite slots so the
            // two draws never collide.
            GrantModifierCarrier(Enemy, ModifierSeedBase + CurrentWave * 7919 + Index);
        }
        SetEnemyDropsLoot(Enemy, Composition.bDropsLoot);
        UBreakerKillTelemetryComponent::AttachTo(Enemy);
        WaveEnemies.Add(Enemy);
    }

    // --- LATTICE ------------------------------------------------------------
    // A ring further out and spread evenly around the arena, so the melee packs
    // push the player ACROSS the ranged fire lanes instead of away from them.
    for (int32 Index = 0; Index < Composition.Lattices; ++Index)
    {
        const float Angle = 360.0f * Index / FMath::Max(1, Composition.Lattices) + 45.0f;
        const FVector SpawnLocation = ArenaCenter
            + FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(Angle, FVector::UpVector) * CombatPocketRadius;
        FActorSpawnParameters RangedParams;
        RangedParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        ABreakerRangedEnemy* Ranged = GetWorld()->SpawnActor<ABreakerRangedEnemy>(
            ABreakerRangedEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, RangedParams);
        if (!Ranged) continue;
        Ranged->ConfigureEncounter(SpawnLocation, Index * 0.9f);
        Ranged->ConfigureWave(AreaLevel);
        SetEnemyDropsLoot(Ranged, Composition.bDropsLoot);
        UBreakerKillTelemetryComponent::AttachTo(Ranged);
        WaveEnemies.Add(Ranged);
    }

    // --- SKIRMISHER ---------------------------------------------------------
    // Placed against COVER, never on a bearing. Wave mode spawns wherever the
    // playtest happens to be standing, so the anchor it finds is whatever the
    // field built nearby — a pocket's cover ring inside a pocket, the sniper
    // lane's hard-cover piece on the lane. In the open it degrades to a Lattice
    // with a longer telegraph, which is strictly worse than a Lattice.
    for (int32 Index = 0; Index < Composition.Skirmishers; ++Index)
    {
        const float Angle = 360.0f * Index / FMath::Max(1, Composition.Skirmishers) + 200.0f;
        const FVector Around = ArenaCenter
            + FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(Angle, FVector::UpVector) * (CombatPocketRadius * 0.8f);
        if (ABreakerSkirmisherEnemy* Skirmisher = SpawnSkirmisherNearCover(Around, Origin, Index * 1.1f))
        {
            Skirmisher->ConfigureWave(AreaLevel);
            SetEnemyDropsLoot(Skirmisher, Composition.bDropsLoot);
            WaveEnemies.Add(Skirmisher);
        }
    }

    // --- WARDEN -------------------------------------------------------------
    // BETWEEN the player and the pack. §2.4's axis is "Wardens punish
    // approaching from the front", and a Warden behind the pack is a Warden the
    // player never has to solve.
    for (int32 Index = 0; Index < Composition.Wardens; ++Index)
    {
        const FVector SpawnLocation = ArenaCenter - Forward * (CombatPocketRadius * 0.6f)
            + FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(Index * 90.0f, FVector::UpVector) * 300.0f;
        FActorSpawnParameters WardenParams;
        WardenParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        ABreakerWardenEnemy* Warden = GetWorld()->SpawnActor<ABreakerWardenEnemy>(
            ABreakerWardenEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, WardenParams);
        if (!Warden) continue;
        Warden->ConfigureEncounter(SpawnLocation, 1.7f + Index);
        Warden->ConfigureWave(AreaLevel);
        SetEnemyDropsLoot(Warden, Composition.bDropsLoot);
        UBreakerKillTelemetryComponent::AttachTo(Warden);
        WaveEnemies.Add(Warden);
    }

    if (Composition.Kind == EBreakerWaveKind::Rest)
    {
        // §4.3: the rest wave exists so wave mode measures COMBAT rather than
        // endurance. Half budget, no elites, loot on, and a breather — the
        // breather is the player's to take, because F4 is what starts the next
        // wave, so what this does is restock and say so.
        RefillPlayerAmmo();
        UE_LOG(LogTemp, Display,
            TEXT("[BreakerGym] wave %d is a REST wave: half budget, no elites, loot enabled, ammo restocked. Take %.0fs before F4."),
            CurrentWave, WaveBudget.RestBreatherSeconds);
    }
}

void ABreakerGameMode::SetEnemyDropsLoot(ABreakerEnemy* Enemy, bool bDrops) const
{
    if (!Enemy) return;

    // §4.3: "Loot only on rest and boss waves. Otherwise the gym becomes a farm
    // and pollutes drop-rate data" — and drop-rate data is one of the two
    // numbers wave mode exists to produce.
    //
    // WHY REFLECTION AND NOT A SETTER. `bDropsLoot` is protected on
    // ABreakerEnemy and there is no mutator; adding one is a one-line change to
    // Combat/, which this lane does not own and which two other agents are
    // editing in parallel. The property is marked BlueprintReadWrite, so it is
    // deliberately writable from outside the class — this reaches it the way a
    // Blueprint would. A missing property WARNS rather than failing silently,
    // because the failure mode is a farm that nobody notices.
    static const FName DropsLootName(TEXT("bDropsLoot"));
    if (FBoolProperty* Property = FindFProperty<FBoolProperty>(ABreakerEnemy::StaticClass(), DropsLootName))
    {
        Property->SetPropertyValue_InContainer(Enemy, bDrops);
        return;
    }
    UE_LOG(LogTemp, Warning,
        TEXT("[BreakerGym] ABreakerEnemy::bDropsLoot not found by reflection; Encounter-Design 4.3's loot rule is not being applied."));
}

FBreakerWaveComposition ABreakerGameMode::GetWaveComposition(int32 WaveIndex) const
{
    // Solo, because solo is the primary balance target and the gym has one
    // player. Party sizes go through the same solver with a different count.
    return UBreakerWaveBudgetLibrary::SolveWave(WaveIndex, 1, WaveBudget);
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
        // IsDeadEnemy(), not a string comparison against a PRESENTATION label.
        // This asked `GetEnemyStateLabel() != "DEAD"`, and that label carried
        // the modifier banner prefixed onto it — so any dead enemy with a
        // modifier answered "WARDED | VOLATILE\nDEAD", compared unequal, and
        // counted as alive permanently. A wave containing a modifier-bearing
        // enemy could not clear. Asking the enemy whether it is dead cannot
        // drift when someone edits a label.
        if (IsValid(Enemy) && !Enemy->IsDeadEnemy()) ++Alive;
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
