#include "Game/BreakerWorldBasics.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

void UBreakerWorldBasics::EnsureWorldLighting(UWorld* World)
{
    if (!World) return;

    // AN AUTHORED LIGHT SUPPRESSES THE RIG. Lvl_FirstPerson (the template map
    // PIE boots) carries its own directional light and must keep looking
    // exactly as it always has; the three empty maps carry none. This one
    // check is also what makes the A6 decision reversible: authoring lighting
    // into a map later retires this code for that map automatically.
    for (TActorIterator<ADirectionalLight> It(World); It; ++It) return;

    // The sun. Pitch -50 gives readable long shadows without the low-sun
    // orange wash; yaw 40 keeps the gym field's forward axis lit from the
    // side so cover reads as volumes rather than silhouettes.
    // O2 PLACEHOLDER: angle and intensity are first-guess values, tuned only
    // to "the owner can see the world"; nobody has judged them as lighting.
    ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(
        FVector::ZeroVector, FRotator(-50.0f, 40.0f, 0.0f));
    if (Sun)
    {
        if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
        {
            SunLight->SetMobility(EComponentMobility::Movable);
            SunLight->SetIntensity(8.0f);   // lux; O2 PLACEHOLDER
            SunLight->SetCastShadows(true);
            // Drives the atmosphere below, which is what turns "a light" into
            // "a sky": sun disc, horizon gradient, blue ambient.
            SunLight->bAtmosphereSunLight = true;
            SunLight->MarkRenderStateDirty();
        }
#if WITH_EDITOR
        Sun->SetActorLabel(TEXT("Runtime_Sun"));
#endif
    }

    // The sky light carries the atmosphere's colour into shadow. Real-time
    // capture (rather than a one-shot capture) because the atmosphere renders
    // after this spawn, and a capture taken now would bake the pre-atmosphere
    // black into every shadowed surface — the exact "everything outside the
    // point-light bubbles is black" the owner reported.
    ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector::ZeroVector, FRotator::ZeroRotator);
    if (Sky)
    {
        if (USkyLightComponent* SkyComp = Sky->GetLightComponent())
        {
            SkyComp->SetMobility(EComponentMobility::Movable);
            SkyComp->bRealTimeCapture = true;
            SkyComp->MarkRenderStateDirty();
        }
        // The atmosphere component rides on the sky light actor rather than
        // its own actor: they are one conceptual thing (the sky), and a
        // second bare AActor would be one more unlabelled mystery in a world
        // outline that is already all runtime spawns.
        if (USkyAtmosphereComponent* Atmosphere = NewObject<USkyAtmosphereComponent>(Sky))
        {
            Atmosphere->SetupAttachment(Sky->GetRootComponent());
            Atmosphere->RegisterComponent();
        }
        // Distance haze in the game's steel-blue (owner: "everything just
        // looks so stale"). A bare atmosphere renders every far surface at
        // full local contrast, which is exactly the flat videogame-void read;
        // a thin height fog gives the world aerial perspective — far things
        // recede into the palette instead of just getting smaller. Start
        // distance keeps the near field (the whole gym arena, the plaza
        // centre) untouched, so nothing in combat range loses contrast.
        // O2 PLACEHOLDER values, judged only by screenshot.
        if (UExponentialHeightFogComponent* Fog = NewObject<UExponentialHeightFogComponent>(Sky))
        {
            Fog->SetupAttachment(Sky->GetRootComponent());
            Fog->SetFogDensity(0.010f);
            Fog->SetFogHeightFalloff(0.35f);
            Fog->SetFogInscatteringColor(FLinearColor(0.33f, 0.44f, 0.58f)); // steel-blue
            Fog->SetStartDistance(2200.0f);
            Fog->SetFogMaxOpacity(0.82f);
            Fog->RegisterComponent();
        }
        // The rig had no post-process control at all, so the fixed-exposure
        // pipeline (r.DefaultFeature.AutoExposure=False project-wide, see
        // DefaultEngine.ini) rendered the sun/atmosphere/fog stack at whatever
        // the raw scene radiance happened to be -- the Anchor plaza's pale
        // concrete and sand under the sun ended up crushed against white
        // with no material separation. An unbound PostProcessComponent on
        // the sky actor is the cleanest zero-asset seam to correct this: it
        // rides on the one actor that already represents "the sky" instead
        // of requiring a separately-placed PostProcessVolume (which would
        // need a bounding volume, another unlabelled actor, and per-map
        // placement), and unbound means it applies everywhere without a
        // volume to size or move. Manual exposure compensation is used
        // (not min/max EV clamps) because auto-exposure is off project-wide;
        // clamps on a disabled feature would do nothing.
        // O2 PLACEHOLDER: exposure bias judged only by screenshot.
        if (UPostProcessComponent* PostProcess = NewObject<UPostProcessComponent>(Sky))
        {
            PostProcess->SetupAttachment(Sky->GetRootComponent());
            PostProcess->bUnbound = true;
            PostProcess->Priority = 0.0f;
            PostProcess->Settings.bOverride_AutoExposureBias = true;
            PostProcess->Settings.AutoExposureBias = -0.7f; // EV; O2 PLACEHOLDER
            // Belt-and-suspenders for the day auto-exposure gets switched
            // back on for this project: keep the adapted range from ever
            // reaching the blown-white end that flagged this fix.
            PostProcess->Settings.bOverride_AutoExposureMinBrightness = true;
            PostProcess->Settings.AutoExposureMinBrightness = 0.2f; // O2 PLACEHOLDER
            PostProcess->Settings.bOverride_AutoExposureMaxBrightness = true;
            PostProcess->Settings.AutoExposureMaxBrightness = 1.5f; // O2 PLACEHOLDER
            PostProcess->RegisterComponent();
        }
#if WITH_EDITOR
        Sky->SetActorLabel(TEXT("Runtime_Sky"));
#endif
    }

    UE_LOG(LogTemp, Log, TEXT("[BreakerWorld] no authored directional light; runtime sun/sky/atmosphere spawned."));
}

void UBreakerWorldBasics::EnsureBootFloor(UWorld* World, const FVector& FloorTopCenter)
{
    if (!World) return;

    // 40 x 40 m: comfortably larger than anything the title menu could let
    // the pawn wander across, deliberately smaller than a space that invites
    // exploring — the front end is a room, not a level. O2 PLACEHOLDER (a
    // layout extent, not balance, flagged under the same discipline).
    const FVector Scale(40.0f, 40.0f, 0.5f);           // engine cube is 100 cm
    const FVector Location = FloorTopCenter - FVector(0.0f, 0.0f, Scale.Z * 100.0f * 0.5f);

    AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
    if (!Floor) return;
    UStaticMeshComponent* Mesh = Floor->GetStaticMeshComponent();
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Mesh->SetWorldScale3D(Scale);
    // Same stock-material-plus-dynamic-instance trick as the hub and gym
    // builders: the basic shape material exposes one "Color" vector param,
    // so no content assets are needed.
    if (UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(BaseMaterial, Mesh))
        {
            // HubPaletteConcrete's value, re-declared because that palette is
            // file-local to BreakerHubBuilder.cpp by design (see the unity-
            // build note there).
            Dynamic->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.33f, 0.35f, 0.30f));
            Mesh->SetMaterial(0, Dynamic);
        }
    }
    Mesh->SetMobility(EComponentMobility::Static);
    Floor->SetActorEnableCollision(true);
    Floor->SetActorTickEnabled(false);
#if WITH_EDITOR
    Floor->SetActorLabel(TEXT("Runtime_BootFloor"));
#endif
}
