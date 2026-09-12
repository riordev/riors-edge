#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Game/BreakerWorldBasics.h"
#include "Game/BreakerWorldLightingMath.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

// O277: the world is lit by one low warm sun and a cool sky, and haze begins
// inside play range. The rule is on bare floats; the rig is then spawned and
// read back so the actor the owner photographs carries the same numbers.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerWorldLightingShippedRigTest,
    "RiorsEdge.World.Lighting.ShippedRig", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerWorldLightingShippedRigTest::RunTest(const FString& Parameters)
{
    using namespace BreakerWorldLighting;
    TestTrue(TEXT("the sun is low and warm"), SunIsLowAndWarm(SunPitchDegrees, SunTemperatureKelvin));
    TestTrue(TEXT("haze begins inside play range"), HazeBeginsInsidePlayRange(FogStartDistanceCm));
    TestFalse(TEXT("a body at arm's reach is not hazed"), HazeBeginsInsidePlayRange(150.0f));
    TestFalse(TEXT("haze beyond the lane is not inside play range"), HazeBeginsInsidePlayRange(2200.0f));
    TestFalse(TEXT("a noon sun is not low"), SunIsLowAndWarm(-50.0f, SunTemperatureKelvin));
    TestFalse(TEXT("a white sun is not warm"), SunIsLowAndWarm(SunPitchDegrees, 6500.0f));
    TestTrue(TEXT("the sky fills cool: its blue channel leads the shadow gain"), GradeShadowsGain().Z > GradeShadowsGain().X);
    TestTrue(TEXT("the grade pulls saturation, never pushes it"), GradeSaturation < 1.0f);

    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("lighting world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };

    UBreakerWorldBasics::EnsureWorldLighting(World);

    ADirectionalLight* Sun = nullptr;
    for (TActorIterator<ADirectionalLight> It(World); It; ++It) Sun = *It;
    if (!TestNotNull(TEXT("the rig spawns a sun"), Sun)) return false;
    TestEqual(TEXT("sun pitch"), Sun->GetActorRotation().Pitch, static_cast<double>(SunPitchDegrees), 0.01);
    if (const UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
    {
        TestEqual(TEXT("sun intensity"), SunLight->Intensity, SunIntensityLux, 0.001f);
        TestTrue(TEXT("sun uses a temperature"), SunLight->bUseTemperature);
        TestEqual(TEXT("sun temperature"), SunLight->Temperature, SunTemperatureKelvin, 0.5f);
        TestTrue(TEXT("sun drives the atmosphere"), SunLight->bAtmosphereSunLight);
    }

    ASkyLight* Sky = nullptr;
    for (TActorIterator<ASkyLight> It(World); It; ++It) Sky = *It;
    if (!TestNotNull(TEXT("the rig spawns a sky"), Sky)) return false;
    if (const USkyLightComponent* SkyComp = Sky->GetLightComponent())
    {
        TestEqual(TEXT("sky intensity"), SkyComp->Intensity, SkyIntensity, 0.001f);
        TestFalse(TEXT("the lower hemisphere is earth, not black"), SkyComp->bLowerHemisphereIsBlack);
    }
    const UExponentialHeightFogComponent* Fog = Sky->FindComponentByClass<UExponentialHeightFogComponent>();
    if (TestNotNull(TEXT("the sky carries the air"), Fog))
    {
        TestEqual(TEXT("fog density"), Fog->FogDensity, FogDensity, 0.0001f);
        TestEqual(TEXT("fog start"), Fog->StartDistance, FogStartDistanceCm, 0.1f);
        TestEqual(TEXT("fog falloff"), Fog->FogHeightFalloff, FogHeightFalloff, 0.001f);
        TestEqual(TEXT("fog max opacity"), Fog->FogMaxOpacity, FogMaxOpacity, 0.001f);
    }
    const UPostProcessComponent* PostProcess = Sky->FindComponentByClass<UPostProcessComponent>();
    if (TestNotNull(TEXT("the sky carries the exposure"), PostProcess))
    {
        TestTrue(TEXT("exposure is unbound"), PostProcess->bUnbound);
        TestEqual(TEXT("exposure bias"), PostProcess->Settings.AutoExposureBias, ExposureBiasEV, 0.001f);
        TestTrue(TEXT("contrast graded"), PostProcess->Settings.bOverride_ColorContrast);
        TestEqual(TEXT("contrast"), static_cast<float>(PostProcess->Settings.ColorContrast.X), GradeContrast, 0.001f);
        TestTrue(TEXT("saturation graded"), PostProcess->Settings.bOverride_ColorSaturation);
        TestTrue(TEXT("shadows gained"), PostProcess->Settings.bOverride_ColorGainShadows);
    }
    return true;
}

#endif
