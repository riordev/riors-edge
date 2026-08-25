#include "UI/BreakerEffectRenderer.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/BreakerGlowMaterial.h"

namespace
{
    // Stock primitives are 100 cm across the bounding box: a unit of scale is
    // a metre. Prefixed against unity-build collisions with the identical
    // constants in BreakerTracerRenderer.cpp — same rule, same reason.
    constexpr float EffectUnitMeshCm = 100.0f;
    const TCHAR* EffectShapeCube = TEXT("/Engine/BasicShapes/Cube.Cube");
    const TCHAR* EffectShapeSphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");

    UStaticMeshComponent* EffectMakePooledMesh(AActor* Owner, USceneComponent* Parent,
        const FString& Name, const TCHAR* MeshPath)
    {
        UStaticMeshComponent* Mesh = Owner->CreateDefaultSubobject<UStaticMeshComponent>(*Name);
        if (!Mesh) return nullptr;
        Mesh->SetupAttachment(Parent);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetGenerateOverlapEvents(false);
        Mesh->SetCastShadow(false);
        Mesh->bReceivesDecals = false;
        Mesh->SetHiddenInGame(true);
        // Every slot is placed in world space; the actor never moves.
        Mesh->SetUsingAbsoluteLocation(true);
        Mesh->SetUsingAbsoluteRotation(true);
        Mesh->SetUsingAbsoluteScale(true);
        if (UStaticMesh* Asset = LoadObject<UStaticMesh>(nullptr, MeshPath))
        {
            Mesh->SetStaticMesh(Asset);
        }
        return Mesh;
    }
}

ABreakerEffectRenderer* ABreakerEffectRenderer::FindOrSpawn(UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<ABreakerEffectRenderer> It(World); It; ++It)
    {
        return *It;
    }
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    return World->SpawnActor<ABreakerEffectRenderer>(
        ABreakerEffectRenderer::StaticClass(), FTransform::Identity, Params);
}

ABreakerEffectRenderer::ABreakerEffectRenderer()
{
    PrimaryActorTick.bCanEverTick = true;
    // After the camera moves, like the tracer: an effect placed against last
    // frame's view shears when the player turns.
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    bReplicates = false;
    SetReplicatingMovement(false);

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    GlowMeshes.Reserve(GlowSlots);
    for (int32 Index = 0; Index < GlowSlots; ++Index)
    {
        GlowMeshes.Add(EffectMakePooledMesh(this, Root,
            FString::Printf(TEXT("EffectGlow%d"), Index), EffectShapeSphere));
    }
    StrokeMeshes.Reserve(StrokeSlots);
    for (int32 Index = 0; Index < StrokeSlots; ++Index)
    {
        StrokeMeshes.Add(EffectMakePooledMesh(this, Root,
            FString::Printf(TEXT("EffectStroke%d"), Index), EffectShapeCube));
    }

    EffectLights.Reserve(EffectLightSlots);
    for (int32 Index = 0; Index < EffectLightSlots; ++Index)
    {
        UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(
            *FString::Printf(TEXT("EffectLight%d"), Index));
        if (Light)
        {
            Light->SetupAttachment(Root);
            Light->SetIntensity(0.0f);
            Light->SetCastShadows(false);
            Light->SetVisibility(false);
            Light->SetUsingAbsoluteLocation(true);
        }
        EffectLights.Add(Light);
    }
}

void ABreakerEffectRenderer::BeginPlay()
{
    Super::BeginPlay();
    // Dynamic instances cannot be created in the constructor. Missing engine
    // content leaves the arrays null and every draw no-ops rather than
    // crashing — the glow helper's contract.
    auto BuildMaterials = [](const TArray<TObjectPtr<UStaticMeshComponent>>& Meshes,
        TArray<TObjectPtr<UMaterialInstanceDynamic>>& OutMaterials)
    {
        OutMaterials.Reset();
        OutMaterials.Reserve(Meshes.Num());
        for (const TObjectPtr<UStaticMeshComponent>& Mesh : Meshes)
        {
            OutMaterials.Add(BreakerUI::MakeGlowMaterial(Mesh));
        }
    };
    BuildMaterials(GlowMeshes, GlowMaterials);
    BuildMaterials(StrokeMeshes, StrokeMaterials);
}

void ABreakerEffectRenderer::AddGlow(const FVector& Center, float RadiusCm, const FLinearColor& Color,
    float Intensity, const BreakerFX::FEffectTiming& Timing, float DelaySeconds)
{
    // Round-robin, oldest-first — with clips of very different lengths the
    // evicted slot is not always the one nearest death (a 6 s ring can lose
    // to a 0.1 s flash claimed later), but under a pool this deep that takes
    // seventeen simultaneous glows, which is not an ability load, it is a
    // bug this policy makes graceful.
    FEffectSlot& Slot = GlowState[NextGlowSlot];
    Slot.A = Center;
    Slot.B = Center;
    Slot.StartTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + FMath::Max(DelaySeconds, 0.0f);
    Slot.Timing = Timing;
    Slot.Color = Color;
    Slot.SizeCm = RadiusCm;
    Slot.Intensity = Intensity;
    Slot.bActive = true;
    NextGlowSlot = (NextGlowSlot + 1) % GlowSlots;
}

void ABreakerEffectRenderer::AddStroke(const FVector& Start, const FVector& End, float ThicknessCm,
    const FLinearColor& Color, float Intensity, const BreakerFX::FEffectTiming& Timing, float DelaySeconds)
{
    FEffectSlot& Slot = StrokeState[NextStrokeSlot];
    Slot.A = Start;
    Slot.B = End;
    Slot.StartTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + FMath::Max(DelaySeconds, 0.0f);
    Slot.Timing = Timing;
    Slot.Color = Color;
    Slot.SizeCm = ThicknessCm;
    Slot.Intensity = Intensity;
    Slot.bActive = true;
    NextStrokeSlot = (NextStrokeSlot + 1) % StrokeSlots;
}

void ABreakerEffectRenderer::AddBlinkLight(const FVector& Center, float AttenuationRadiusCm,
    const FLinearColor& Color, float Intensity, const BreakerFX::FEffectTiming& Timing, float DelaySeconds)
{
    FEffectLightSlot& Slot = LightState[NextLightSlot];
    Slot.Center = Center;
    Slot.StartTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + FMath::Max(DelaySeconds, 0.0f);
    Slot.Timing = Timing;
    Slot.Color = Color;
    Slot.AttenuationRadiusCm = AttenuationRadiusCm;
    Slot.Intensity = Intensity;
    Slot.bActive = true;
    NextLightSlot = (NextLightSlot + 1) % EffectLightSlots;
}

void ABreakerEffectRenderer::Hide(UStaticMeshComponent* Mesh)
{
    if (Mesh && !Mesh->bHiddenInGame) Mesh->SetHiddenInGame(true);
}

void ABreakerEffectRenderer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    const UWorld* World = GetWorld();
    if (!World) return;
    const double Now = World->GetTimeSeconds();

    for (int32 Index = 0; Index < GlowSlots; ++Index)
    {
        FEffectSlot& Slot = GlowState[Index];
        UStaticMeshComponent* Mesh = GlowMeshes.IsValidIndex(Index) ? GlowMeshes[Index].Get() : nullptr;
        if (!Slot.bActive)
        {
            Hide(Mesh);
            continue;
        }
        const BreakerFX::FEffectSample Sample = BreakerFX::SampleEffect(
            Slot.Timing, static_cast<float>(Now - Slot.StartTime));
        if (Sample.bFinished)
        {
            Slot.bActive = false;
            Hide(Mesh);
            continue;
        }
        if (!Sample.bVisible || !Mesh || Slot.SizeCm < 0.5f)
        {
            Hide(Mesh);
            continue;
        }
        Mesh->SetWorldLocation(Slot.A);
        Mesh->SetWorldScale3D(FVector(Slot.SizeCm * 2.0f / EffectUnitMeshCm));
        BreakerUI::SetGlowColor(GlowMaterials.IsValidIndex(Index) ? GlowMaterials[Index].Get() : nullptr,
            Slot.Color, Slot.Intensity * Sample.Alpha);
        if (Mesh->bHiddenInGame) Mesh->SetHiddenInGame(false);
    }

    for (int32 Index = 0; Index < StrokeSlots; ++Index)
    {
        FEffectSlot& Slot = StrokeState[Index];
        UStaticMeshComponent* Mesh = StrokeMeshes.IsValidIndex(Index) ? StrokeMeshes[Index].Get() : nullptr;
        if (!Slot.bActive)
        {
            Hide(Mesh);
            continue;
        }
        const BreakerFX::FEffectSample Sample = BreakerFX::SampleEffect(
            Slot.Timing, static_cast<float>(Now - Slot.StartTime));
        if (Sample.bFinished)
        {
            Slot.bActive = false;
            Hide(Mesh);
            continue;
        }
        const FVector Delta = Slot.B - Slot.A;
        const float Length = static_cast<float>(Delta.Size());
        if (!Sample.bVisible || !Mesh || Length < 1.0f)
        {
            Hide(Mesh);
            continue;
        }
        Mesh->SetWorldLocationAndRotation((Slot.A + Slot.B) * 0.5,
            FRotationMatrix::MakeFromX(Delta / Length).Rotator());
        Mesh->SetWorldScale3D(FVector(Length / EffectUnitMeshCm,
            Slot.SizeCm / EffectUnitMeshCm, Slot.SizeCm / EffectUnitMeshCm));
        BreakerUI::SetGlowColor(StrokeMaterials.IsValidIndex(Index) ? StrokeMaterials[Index].Get() : nullptr,
            Slot.Color, Slot.Intensity * Sample.Alpha);
        if (Mesh->bHiddenInGame) Mesh->SetHiddenInGame(false);
    }

    for (int32 Index = 0; Index < EffectLightSlots; ++Index)
    {
        FEffectLightSlot& Slot = LightState[Index];
        UPointLightComponent* Light = EffectLights.IsValidIndex(Index) ? EffectLights[Index].Get() : nullptr;
        if (!Light) continue;
        if (!Slot.bActive)
        {
            if (Light->IsVisible()) Light->SetVisibility(false);
            continue;
        }
        const BreakerFX::FEffectSample Sample = BreakerFX::SampleEffect(
            Slot.Timing, static_cast<float>(Now - Slot.StartTime));
        if (Sample.bFinished)
        {
            Slot.bActive = false;
            Light->SetVisibility(false);
            continue;
        }
        if (!Sample.bVisible)
        {
            if (Light->IsVisible()) Light->SetVisibility(false);
            continue;
        }
        Light->SetWorldLocation(Slot.Center);
        Light->SetLightColor(Slot.Color);
        Light->SetAttenuationRadius(Slot.AttenuationRadiusCm);
        Light->SetIntensity(Slot.Intensity * Sample.Alpha);
        if (!Light->IsVisible()) Light->SetVisibility(true);
    }
}
