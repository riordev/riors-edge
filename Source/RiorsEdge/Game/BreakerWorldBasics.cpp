#include "Game/BreakerWorldBasics.h"
#include "Game/BreakerWorldLightingMath.h"

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

    // The sun: one low warm sun (O277). Every number is in
    // BreakerWorldLightingMath.h, where the ShippedRig pin reads it; this
    // actor only applies them.
    ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(
        FVector::ZeroVector, FRotator(BreakerWorldLighting::SunPitchDegrees, BreakerWorldLighting::SunYawDegrees, 0.0f));
    if (Sun)
    {
        // The light component ships a default pitch of its own, and a spawn
        // rotation composes with it; the world rotation is set explicitly so
        // the pitch the pin reads is the pitch the sky gets.
        Sun->SetActorRotation(FRotator(BreakerWorldLighting::SunPitchDegrees, BreakerWorldLighting::SunYawDegrees, 0.0f));
        if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
        {
            SunLight->SetMobility(EComponentMobility::Movable);
            SunLight->SetIntensity(BreakerWorldLighting::SunIntensityLux);
            SunLight->SetUseTemperature(true);
            SunLight->SetTemperature(BreakerWorldLighting::SunTemperatureKelvin);
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
            // The cool fill against the warm sun (O277); the lower hemisphere
            // is earth so bounce under overhangs is not blue.
            SkyComp->SetIntensity(BreakerWorldLighting::SkyIntensity);
            SkyComp->bLowerHemisphereIsBlack = false;
            SkyComp->SetLowerHemisphereColor(BreakerWorldLighting::SkyLowerHemisphereColor());
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
        // The air has depth (O277): haze begins inside play range, so the
        // far end of the yard recedes into a cool band instead of sitting at
        // full local contrast. A bare atmosphere is the flat videogame-void
        // read the owner named. Numbers in BreakerWorldLightingMath.h.
        if (UExponentialHeightFogComponent* Fog = NewObject<UExponentialHeightFogComponent>(Sky))
        {
            Fog->SetupAttachment(Sky->GetRootComponent());
            Fog->SetFogDensity(BreakerWorldLighting::FogDensity);
            Fog->SetFogHeightFalloff(BreakerWorldLighting::FogHeightFalloff);
            Fog->SetFogInscatteringColor(BreakerWorldLighting::FogInscatteringColor());
            Fog->SetStartDistance(BreakerWorldLighting::FogStartDistanceCm);
            Fog->SetFogMaxOpacity(BreakerWorldLighting::FogMaxOpacity);
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
        // Exposure and grade numbers in BreakerWorldLightingMath.h (O277).
        if (UPostProcessComponent* PostProcess = NewObject<UPostProcessComponent>(Sky))
        {
            PostProcess->SetupAttachment(Sky->GetRootComponent());
            PostProcess->bUnbound = true;
            PostProcess->Priority = 0.0f;
            PostProcess->Settings.bOverride_AutoExposureBias = true;
            PostProcess->Settings.AutoExposureBias = BreakerWorldLighting::ExposureBiasEV;
            // The grade: a touch of contrast, a slight pull on saturation,
            // and shadows gained toward the sky's blue so shadow is cool,
            // never black.
            PostProcess->Settings.bOverride_ColorContrast = true;
            PostProcess->Settings.ColorContrast = FVector4(BreakerWorldLighting::GradeContrast, BreakerWorldLighting::GradeContrast, BreakerWorldLighting::GradeContrast, 1.0f);
            PostProcess->Settings.bOverride_ColorSaturation = true;
            PostProcess->Settings.ColorSaturation = FVector4(BreakerWorldLighting::GradeSaturation, BreakerWorldLighting::GradeSaturation, BreakerWorldLighting::GradeSaturation, 1.0f);
            PostProcess->Settings.bOverride_ColorGainShadows = true;
            PostProcess->Settings.ColorGainShadows = BreakerWorldLighting::GradeShadowsGain();
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
