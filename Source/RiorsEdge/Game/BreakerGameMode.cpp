#include "Game/BreakerGameMode.h"

#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerTargetDummy.h"
#include "Combat/BreakerEnemy.h"
#include "Interaction/BreakerNPC.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Math/RandomStream.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/PlayerController.h"
#include "UI/BreakerPlaytestHUD.h"
#include "EngineUtils.h"
#include "UObject/UObjectGlobals.h"

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
}

void ABreakerGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);
    if (bPlaytestTargetsSpawned || !NewPlayer || !NewPlayer->GetPawn() || !GetWorld()) return;
    SpawnSafeZone(NewPlayer->GetPawn());
    SpawnAnchorCamp(NewPlayer->GetPawn());
    SpawnPlaytestTargets(NewPlayer->GetPawn());
    SpawnMovementCourse(NewPlayer->GetPawn());
    SpawnCombatEncounter(NewPlayer->GetPawn());
    SpawnWorldDressing(NewPlayer->GetPawn());
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

void ABreakerGameMode::SpawnPlaytestTargets(const APawn* Pawn)
{
    if (!Pawn || !GetWorld()) return;

    const FVector Origin = Pawn->GetActorLocation();
    const FVector Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
    const FVector Right = Pawn->GetActorRightVector().GetSafeNormal2D();
    const FVector TargetOffsets[] =
    {
        Forward * 1200.0f - Right * 300.0f,
        Forward * 2400.0f + Right * 350.0f,
        Forward * 4500.0f,
        Forward * 2100.0f - Right * 850.0f
    };
    const EBreakerTargetProfile Profiles[] =
    {
        EBreakerTargetProfile::Health,
        EBreakerTargetProfile::Shielded,
        EBreakerTargetProfile::Armored,
        EBreakerTargetProfile::Moving
    };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(TargetOffsets); ++Index)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        if (ABreakerTargetDummy* Target = GetWorld()->SpawnActor<ABreakerTargetDummy>(ABreakerTargetDummy::StaticClass(), Origin + TargetOffsets[Index], FRotator::ZeroRotator, Params))
        {
            Target->ConfigureProfile(Profiles[Index]);
        }
    }
    bPlaytestTargetsSpawned = true;
}

void ABreakerGameMode::SpawnMovementCourse(const APawn* Pawn)
{
    if (!Pawn || !GetWorld()) return;
    const FVector Origin = Pawn->GetActorLocation();
    const FVector Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
    const FVector Right = Pawn->GetActorRightVector().GetSafeNormal2D();

    // Mantle staircase: 50 / 100 / 145 cm tops.
    SpawnGymBlock(GetWorld(), Origin - Right * 900.0f + Forward * 900.0f + FVector(0, 0, 25), FVector(1.5f, 1.5f, 0.5f), FRotator::ZeroRotator, PaletteEarth);
    SpawnGymBlock(GetWorld(), Origin - Right * 900.0f + Forward * 1250.0f + FVector(0, 0, 50), FVector(1.5f, 1.5f, 1.0f), FRotator::ZeroRotator, PaletteEarth);
    SpawnGymBlock(GetWorld(), Origin - Right * 900.0f + Forward * 1650.0f + FVector(0, 0, 72.5f), FVector(1.5f, 1.5f, 1.45f), FRotator::ZeroRotator, PaletteEarth);

    // Dash distance markers and staggered collision gates.
    for (int32 Marker = 1; Marker <= 4; ++Marker)
    {
        SpawnGymBlock(GetWorld(), Origin + Right * 1050.0f + Forward * (Marker * 500.0f) + FVector(0, 0, 60), FVector(0.08f, 2.0f, 1.2f), FRotator::ZeroRotator, PaletteStone);
    }

    // Gap platforms and parallel walls for wall-ride / wall-jump checks.
    SpawnGymBlock(GetWorld(), Origin + Forward * 3000.0f - Right * 1300.0f + FVector(0, 0, 15), FVector(4.0f, 3.0f, 0.3f), FRotator::ZeroRotator, PaletteMoss);
    SpawnGymBlock(GetWorld(), Origin + Forward * 4200.0f - Right * 1300.0f + FVector(0, 0, 15), FVector(4.0f, 3.0f, 0.3f), FRotator::ZeroRotator, PaletteMoss);
    SpawnGymBlock(GetWorld(), Origin + Forward * 3400.0f + Right * 1350.0f + FVector(0, 0, 180), FVector(12.0f, 0.25f, 3.6f), FRotator::ZeroRotator, PaletteConcrete);
    SpawnGymBlock(GetWorld(), Origin + Forward * 3400.0f + Right * 2050.0f + FVector(0, 0, 180), FVector(12.0f, 0.25f, 3.6f), FRotator::ZeroRotator, PaletteConcrete);

    // Flat and sloped slide lanes.
    SpawnGymBlock(GetWorld(), Origin - Forward * 900.0f - Right * 1200.0f + FVector(0, 0, 20), FVector(8.0f, 2.5f, 0.2f), FRotator::ZeroRotator, PaletteEarth);
    SpawnGymBlock(GetWorld(), Origin - Forward * 2100.0f + Right * 1200.0f + FVector(0, 0, 240), FVector(8.0f, 2.5f, 0.2f), FRotator(0, 0, -12.0f), PaletteEarth);
}

void ABreakerGameMode::SpawnAnchorCamp(const APawn* Pawn)
{
    if (!Pawn || !GetWorld()) return;
    const FVector Origin = Pawn->GetActorLocation() - FVector(0.0f, 0.0f, 88.0f);
    const FVector Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
    const FVector Right = Pawn->GetActorRightVector().GetSafeNormal2D();

    // A small Anchor camp behind the spawn pad: plaza, back wall, a Forge
    // prop, and two talkable NPCs — the seed of the hub.
    const FVector CampCenter = Origin - Forward * 1400.0f;
    SpawnGymBlock(GetWorld(), CampCenter + FVector(0, 0, 8), FVector(14.0f, 14.0f, 0.15f), FRotator::ZeroRotator, PaletteEarth);
    SpawnGymBlock(GetWorld(), CampCenter - Forward * 700.0f + FVector(0, 0, 180), FVector(14.0f, 0.3f, 3.6f), FRotator::ZeroRotator, PaletteConcrete);
    if (AStaticMeshActor* Forge = SpawnGymBlock(GetWorld(), CampCenter - Forward * 500.0f - Right * 450.0f + FVector(0, 0, 110), FVector(1.6f, 1.6f, 2.2f), FRotator::ZeroRotator, PaletteRust))
    {
        AttachPropLight(Forge, FVector(0, 0, 40.0f), FLinearColor(1.0f, 0.62f, 0.26f), 900.0f, 700.0f);  // Forge glow (light 1/6)
    }
    SpawnGymBlock(GetWorld(), CampCenter - Forward * 500.0f + Right * 480.0f + FVector(0, 0, 60), FVector(2.4f, 1.2f, 1.2f), FRotator::ZeroRotator, PaletteOffWhite);   // supply crates

    ABreakerNPC::SpawnForgeKeeper(GetWorld(), CampCenter - Forward * 420.0f - Right * 450.0f + FVector(0, 0, 12), (Forward).Rotation());
    ABreakerNPC::SpawnQuartermaster(GetWorld(), CampCenter - Forward * 420.0f + Right * 480.0f + FVector(0, 0, 12), (Forward).Rotation());

    // Watchtower platforms flanking the route out — vertical playground and
    // sniper perches for the encounter approach.
    SpawnGymBlock(GetWorld(), Origin + Forward * 1800.0f - Right * 2100.0f + FVector(0, 0, 240), FVector(3.0f, 3.0f, 0.3f), FRotator::ZeroRotator, PaletteConcrete);
    SpawnGymBlock(GetWorld(), Origin + Forward * 1800.0f - Right * 2100.0f + FVector(0, 0, 110), FVector(0.4f, 0.4f, 2.2f), FRotator::ZeroRotator, PaletteStone);
    SpawnGymBlock(GetWorld(), Origin + Forward * 2600.0f + Right * 2200.0f + FVector(0, 0, 340), FVector(3.0f, 3.0f, 0.3f), FRotator::ZeroRotator, PaletteConcrete);
    SpawnGymBlock(GetWorld(), Origin + Forward * 2600.0f + Right * 2200.0f + FVector(0, 0, 160), FVector(0.4f, 0.4f, 3.2f), FRotator::ZeroRotator, PaletteStone);

    // Arena boundary markers around the elite's ground.
    for (int32 Marker = 0; Marker < 6; ++Marker)
    {
        const float Angle = Marker * 60.0f;
        const FVector Offset = Forward.RotateAngleAxis(Angle, FVector::UpVector) * 900.0f;
        SpawnGymBlock(GetWorld(), Origin + Forward * (SafeZoneRadius + 2400.0f) + Offset + FVector(0, 0, 90), FVector(0.3f, 0.3f, 1.8f), FRotator::ZeroRotator, PaletteStone);
    }
}

void ABreakerGameMode::SpawnSafeZone(const APawn* Pawn)
{
    if (!Pawn || !GetWorld()) return;
    SafeZoneCenter = Pawn->GetActorLocation() - FVector(0.0f, 0.0f, 88.0f);
    bSafeZoneSet = true;

    // Visible pad so the boundary reads at a glance: wide flat cylinder with
    // a thin rim ring at the zone radius.
    AStaticMeshActor* Pad = GetWorld()->SpawnActor<AStaticMeshActor>(SafeZoneCenter + FVector(0, 0, 2.0f), FRotator::ZeroRotator);
    if (Pad)
    {
        UStaticMeshComponent* Mesh = Pad->GetStaticMeshComponent();
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
        Mesh->SetWorldScale3D(FVector(SafeZoneRadius / 50.0f, SafeZoneRadius / 50.0f, 0.04f));
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        // Object chroma: the pad is suppression hardware, so it carries the
        // reserved saturated teal. Nothing else does except the pylon below.
        ApplyShapeColor(Mesh, PaletteRiftTeal);
        Mesh->SetMobility(EComponentMobility::Static);
        Pad->SetActorLabel(TEXT("Runtime_SafeZone"));
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

void ABreakerGameMode::SpawnCombatEncounter(const APawn* Pawn)
{
    if (!Pawn || !GetWorld()) return;
    const FVector Origin = Pawn->GetActorLocation();
    const FVector Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
    const FVector Right = Pawn->GetActorRightVector().GetSafeNormal2D();
    const float LateralOffsets[] = { -450.0f, 0.0f, 450.0f };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        // Spawn well outside the safe zone so the fight starts on the
        // player's terms.
        const FVector SpawnLocation = Origin + Forward * (SafeZoneRadius + 1200.0f + Index * 300.0f) + Right * LateralOffsets[Index];
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        if (ABreakerEnemy* Enemy = GetWorld()->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, Params))
        {
            Enemy->ConfigureEncounter(SpawnLocation, Index * 1.7f);
        }
    }

    // One elite anchors the back of the pack: tougher, harder-hitting, and
    // guaranteed Exceptional-or-better drops.
    const FVector EliteLocation = Origin + Forward * (SafeZoneRadius + 2400.0f);
    FActorSpawnParameters EliteParams;
    EliteParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    if (ABreakerEnemy* Elite = GetWorld()->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), EliteLocation, FRotator::ZeroRotator, EliteParams))
    {
        Elite->ConfigureEncounter(EliteLocation, 0.9f);
        Elite->ConfigureElite();
    }
}

void ABreakerGameMode::SpawnWorldDressing(const APawn* Pawn)
{
    if (!Pawn || !GetWorld()) return;
    const FVector Origin = Pawn->GetActorLocation() - FVector(0.0f, 0.0f, 88.0f);
    const FVector Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
    const FVector Right = Pawn->GetActorRightVector().GetSafeNormal2D();

    // One seed drives every dressing decision so the gym looks identical each
    // run and screenshots stay comparable between playtests.
    FRandomStream Stream(20260812);
    SpawnRuins(Origin, Forward, Right, Stream);
    SpawnScatteredTech(Origin, Forward, Right, Stream);
    SpawnOvergrowth(Origin, Forward, Right, Stream);
}

void ABreakerGameMode::SpawnOvergrowth(const FVector& Origin, const FVector& Forward, const FVector& Right, FRandomStream& Stream)
{
    // Vegetation clusters: squashed spheres read as bushes, thin tall cones as
    // reeds and saplings. All non-colliding, so movement tests are unaffected.
    const FVector ClusterAnchors[] =
    {
        Origin - Forward * 1400.0f - Right * 750.0f,     // camp edge
        Origin - Forward * 1900.0f + Right * 700.0f,     // camp edge
        Origin - Forward * 800.0f - Right * 1750.0f,     // slide lane edge
        Origin + Forward * 1200.0f - Right * 1500.0f,    // staircase flank
        Origin + Forward * 2400.0f + Right * 1750.0f,    // wall corridor edge
        Origin + Forward * 3600.0f - Right * 1900.0f,    // gap platform edge
        Origin + Forward * 1800.0f - Right * 2100.0f,    // watchtower base
        Origin + Forward * 2600.0f + Right * 2200.0f,    // watchtower base
        Origin + Forward * (SafeZoneRadius + 1500.0f) - Right * 1500.0f,  // ruin base
        Origin + Forward * (SafeZoneRadius + 2600.0f) + Right * 1600.0f   // arena rim
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

void ABreakerGameMode::SpawnRuins(const FVector& Origin, const FVector& Forward, const FVector& Right, FRandomStream& Stream)
{
    // Broken walls: overlapping offset boxes at odd angles, partially sunken,
    // weathered palette. These DO collide — they are playable cover.
    const FVector WallAnchors[] =
    {
        Origin + Forward * (SafeZoneRadius + 1500.0f) - Right * 1400.0f,
        Origin + Forward * (SafeZoneRadius + 2100.0f) + Right * 1500.0f,
        Origin - Forward * 2300.0f - Right * 900.0f
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

    // Collapsed arch: two leaning legs and a fallen span across them.
    const FVector ArchBase = Origin + Forward * (SafeZoneRadius + 1900.0f) - Right * 300.0f;
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
        const FVector Offset = Forward.RotateAngleAxis(Angle, FVector::UpVector) * Stream.FRandRange(1100.0f, 1500.0f);
        SpawnShape(GetWorld(), ShapeCube,
            Origin + Forward * (SafeZoneRadius + 2400.0f) + Offset + FVector(0, 0, Stream.FRandRange(10.0f, 40.0f)),
            FVector(Stream.FRandRange(2.0f, 3.6f), Stream.FRandRange(1.6f, 2.8f), 0.22f),
            FRotator(Stream.FRandRange(-8.0f, 8.0f), Angle, Stream.FRandRange(-8.0f, 8.0f)),
            Stream.FRand() < 0.5f ? PaletteStone : PaletteConcrete, true, TEXT("Runtime_Ruin"));
    }
}

void ABreakerGameMode::SpawnScatteredTech(const FVector& Origin, const FVector& Forward, const FVector& Right, FRandomStream& Stream)
{
    // O24's "slight sci-fi": functional, weathered, out of place. Amber and
    // off-white only — teal stays reserved for the pad and the pylon.
    const FVector CampCenter = Origin - Forward * 1400.0f;

    // 1. Leaning monolith panel at the camp mouth.
    SpawnShape(GetWorld(), ShapeCube, CampCenter + Forward * 620.0f - Right * 620.0f + FVector(0, 0, 210.0f),
        FVector(1.6f, 0.22f, 4.2f), FRotator(0.0f, 24.0f, -13.0f), PaletteOffWhite, true, TEXT("Runtime_TechProp"));

    // 2. Generator cylinder with a low amber lamp.
    if (AStaticMeshActor* Generator = SpawnShape(GetWorld(), ShapeCylinder, CampCenter + Forward * 500.0f + Right * 700.0f + FVector(0, 0, 70.0f),
        FVector(0.9f, 0.9f, 1.4f), FRotator(0.0f, 0.0f, 4.0f), PaletteRust, true, TEXT("Runtime_TechProp")))
    {
        AttachPropLight(Generator, FVector(0, 0, 90.0f), FLinearColor(1.0f, 0.66f, 0.30f), 800.0f, 650.0f);  // light 3/6
    }

    // 3. Crate stack on the route out.
    SpawnShape(GetWorld(), ShapeCube, Origin + Forward * 900.0f + Right * 1650.0f + FVector(0, 0, 55.0f),
        FVector(1.1f, 1.1f, 1.1f), FRotator(0.0f, 18.0f, 0.0f), PaletteOffWhite, true, TEXT("Runtime_TechProp"));
    SpawnShape(GetWorld(), ShapeCube, Origin + Forward * 960.0f + Right * 1590.0f + FVector(0, 0, 150.0f),
        FVector(0.8f, 0.8f, 0.8f), FRotator(0.0f, -31.0f, 5.0f), PaletteRust, true, TEXT("Runtime_TechProp"));

    // 4. Broken antenna ring: half-buried torus faked from tilted cylinders.
    const FVector RingBase = Origin + Forward * 2900.0f - Right * 1750.0f;
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
            Origin + Forward * (SafeZoneRadius + 1350.0f) - Right * (1000.0f + Drum * 180.0f) + FVector(0, 0, 40.0f),
            FVector(0.5f, 0.5f, 0.8f), FRotator(Stream.FRandRange(70.0f, 96.0f), Stream.FRandRange(0.0f, 360.0f), 0.0f),
            Drum == 1 ? PaletteOffWhite : PaletteRust, true, TEXT("Runtime_TechProp"));
    }

    // 6. Half-sunken relay cone beside the arena approach, one dim lamp.
    if (AStaticMeshActor* Relay = SpawnShape(GetWorld(), ShapeCone, Origin + Forward * (SafeZoneRadius + 900.0f) + Right * 950.0f + FVector(0, 0, 60.0f),
        FVector(1.4f, 1.4f, 1.6f), FRotator(0.0f, 0.0f, 17.0f), PaletteAmber, true, TEXT("Runtime_TechProp")))
    {
        AttachPropLight(Relay, FVector(0, 0, 110.0f), FLinearColor(1.0f, 0.70f, 0.32f), 500.0f, 500.0f);  // light 5/6
    }
}

void ABreakerGameMode::StartNextWave()
{
    if (!GetWorld() || IsWaveActive()) return;
    APawn* PlayerPawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
    if (!PlayerPawn) return;

    ++CurrentWave;
    WaveEnemies.RemoveAll([](const TObjectPtr<ABreakerEnemy>& Enemy) { return !IsValid(Enemy); });

    const FVector Origin = PlayerPawn->GetActorLocation();
    const FVector Forward = PlayerPawn->GetActorForwardVector().GetSafeNormal2D();
    const FVector ArenaCenter = Origin + Forward * (SafeZoneRadius + 2400.0f);
    // Dense packs by design: AoE, on-death chains, and multikill procs need
    // crowds to feel like anything. Clusters of ~4 around the arena ring.
    const int32 EnemyCount = FMath::Min(4 + CurrentWave * 3, 24);
    const bool bEliteWave = CurrentWave % 3 == 0;

    for (int32 Index = 0; Index < EnemyCount; ++Index)
    {
        const int32 Pack = Index / 4;
        const float PackAngle = 360.0f * Pack / FMath::Max(1, (EnemyCount + 3) / 4);
        const FVector PackCenter = ArenaCenter + FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(PackAngle, FVector::UpVector) * 700.0f;
        const FVector SpawnLocation = PackCenter + FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(Index * 90.0f, FVector::UpVector) * 160.0f;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        if (ABreakerEnemy* Enemy = GetWorld()->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, Params))
        {
            Enemy->ConfigureEncounter(SpawnLocation, Index * 1.3f);
            // Later waves climb in level so drops and TTK data climb too.
            Enemy->ConfigureWave(10 + CurrentWave * 2);
            if (bEliteWave && Index == 0) Enemy->ConfigureElite();
            WaveEnemies.Add(Enemy);
        }
    }
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
            SpawnPlaytestTargets(PC->GetPawn());
            SpawnCombatEncounter(PC->GetPawn());
            break;
        }
    }
}
