#include "UI/BreakerEffectRenderer.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
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

    // The moment pool. Dormant until PlayMoment hands a slot a system: no
    // asset in the constructor, no auto-activate, never auto-destroyed — a
    // slot outlives every effect it plays, which is the whole point of a pool.
    MomentComponents.Reserve(MomentSlots);
    for (int32 Index = 0; Index < MomentSlots; ++Index)
    {
        UNiagaraComponent* Niagara = CreateDefaultSubobject<UNiagaraComponent>(
            *FString::Printf(TEXT("EffectMoment%d"), Index));
        if (Niagara)
        {
            Niagara->SetupAttachment(Root);
            Niagara->SetAutoActivate(false);
            Niagara->SetAutoDestroy(false);
            Niagara->SetUsingAbsoluteLocation(true);
            Niagara->SetUsingAbsoluteRotation(true);
            Niagara->SetUsingAbsoluteScale(true);
            Niagara->SetCastShadow(false);
        }
        MomentComponents.Add(Niagara);
    }
    MomentSystems.SetNum(BreakerFX::EffectMomentCount);
}

UNiagaraSystem* ABreakerEffectRenderer::ResolveMomentSystem(EBreakerEffectMoment Moment)
{
    const int32 Index = static_cast<int32>(Moment);
    if (Index < 0 || Index >= BreakerFX::EffectMomentCount) return nullptr;
    if (MomentSystems.Num() != BreakerFX::EffectMomentCount) MomentSystems.SetNum(BreakerFX::EffectMomentCount);
    if (bMomentProbed[Index]) return MomentSystems[Index].Get();

    // Probe once. The path is fixed by the math header so the owner names a
    // system after its moment and nothing here has to learn it. A miss is
    // the normal state until the asset is authored and is not logged: one
    // line per session when a system IS found is the useful signal.
    bMomentProbed[Index] = true;
    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *BreakerFX::MomentAssetPath(Moment));
    MomentSystems[Index] = System;
    if (System)
    {
        UE_LOG(LogTemp, Log, TEXT("[BreakerFX] %s: authored Niagara system loaded; the pooled fallback stands down."),
            BreakerFX::MomentAssetName(Moment));
    }
    return System;
}

int32 ABreakerEffectRenderer::PlayMoment(EBreakerEffectMoment Moment, const FVector& Location,
    const FVector& Direction, const FLinearColor& Color, float DelaySeconds)
{
    if (DelaySeconds <= KINDA_SMALL_NUMBER) return PlayMomentNow(Moment, Location, Direction, Color);

    // A moment that would draw nothing (unauthored, and its fallback is
    // somebody else's primitive) must not occupy a pending slot: a shotgun's
    // landed pellets would otherwise evict the death scheduled beside them.
    if (!ResolveMomentSystem(Moment) && !BreakerFX::MomentFallback(Moment).bDrawn) return 0;

    // A Niagara component cannot be told "start in 0.18 s", so a scheduled
    // moment waits in a fixed ring the tick drains. Oldest-first like every
    // pool here: nine deaths landing inside one tracer flight is not a load,
    // it is a bug this makes graceful.
    FPendingMoment& Pending = PendingMoments[NextPendingMoment];
    Pending.Moment = Moment;
    Pending.Location = Location;
    Pending.Direction = Direction;
    Pending.Color = Color;
    Pending.FireTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + DelaySeconds;
    Pending.bActive = true;
    NextPendingMoment = (NextPendingMoment + 1) % PendingMomentSlots;
    return 0;
}

int32 ABreakerEffectRenderer::PlayMomentNow(EBreakerEffectMoment Moment, const FVector& Location,
    const FVector& Direction, const FLinearColor& Color)
{
    if (UNiagaraSystem* System = ResolveMomentSystem(Moment))
    {
        UNiagaraComponent* Niagara = MomentComponents.IsValidIndex(NextMomentSlot)
            ? MomentComponents[NextMomentSlot].Get() : nullptr;
        NextMomentSlot = (NextMomentSlot + 1) % MomentSlots;
        if (Niagara)
        {
            const FVector Facing = Direction.IsNearlyZero() ? FVector::UpVector : Direction.GetSafeNormal();
            if (Niagara->GetAsset() != System) Niagara->SetAsset(System);
            Niagara->SetWorldLocationAndRotation(Location, FRotationMatrix::MakeFromX(Facing).Rotator());
            // The one parameter every moment system is asked to expose. A
            // system without it simply ignores the write and plays its
            // authored colour — the O179 law is then the author's to keep.
            Niagara->SetVariableLinearColor(TEXT("Color"), Color);
            Niagara->Activate(/*bReset*/ true);
            return 0;
        }
    }

    // Not authored yet: the pooled primitives stand in.
    const BreakerFX::FMomentFallback Fallback = BreakerFX::MomentFallback(Moment);
    if (!Fallback.bDrawn) return 0;
    const int32 Handle = AddGlow(Location, Fallback.RadiusCm, Color, Fallback.Intensity, Fallback.Timing);
    if (Fallback.LightRadiusCm > 0.0f)
    {
        AddBlinkLight(Location, Fallback.LightRadiusCm, Color, Fallback.LightIntensity, Fallback.Timing);
    }
    return Handle;
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

int32 ABreakerEffectRenderer::AddGlow(const FVector& Center, float RadiusCm, const FLinearColor& Color,
    float Intensity, const BreakerFX::FEffectTiming& Timing, float DelaySeconds)
{
    // Round-robin, oldest-first — with clips of very different lengths the
    // evicted slot is not always the one nearest death (a 6 s ring can lose
    // to a 0.1 s flash claimed later), but under a pool this deep that takes
    // seventeen simultaneous glows, which is not an ability load, it is a
    // bug this policy makes graceful.
    FEffectSlot& Slot = GlowState[NextGlowSlot];
    Slot = FEffectSlot();
    Slot.A = Center;
    Slot.B = Center;
    Slot.StartTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + FMath::Max(DelaySeconds, 0.0f);
    Slot.Timing = Timing;
    Slot.Color = Color;
    Slot.SizeCm = RadiusCm;
    Slot.Intensity = Intensity;
    Slot.bActive = true;
    Slot.Serial = NextSerial++;
    NextGlowSlot = (NextGlowSlot + 1) % GlowSlots;
    return Slot.Serial;
}

int32 ABreakerEffectRenderer::AddStroke(const FVector& Start, const FVector& End, float ThicknessCm,
    const FLinearColor& Color, float Intensity, const BreakerFX::FEffectTiming& Timing, float DelaySeconds)
{
    FEffectSlot& Slot = StrokeState[NextStrokeSlot];
    Slot = FEffectSlot();
    Slot.A = Start;
    Slot.B = End;
    Slot.StartTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + FMath::Max(DelaySeconds, 0.0f);
    Slot.Timing = Timing;
    Slot.Color = Color;
    Slot.SizeCm = ThicknessCm;
    Slot.Intensity = Intensity;
    Slot.bActive = true;
    Slot.Serial = NextSerial++;
    NextStrokeSlot = (NextStrokeSlot + 1) % StrokeSlots;
    return Slot.Serial;
}

int32 ABreakerEffectRenderer::AddBeam(AActor* SourceAnchor, AActor* TargetAnchor, float ThicknessCm,
    const FLinearColor& Color, float Intensity, const BreakerFX::FEffectTiming& Timing, float AnchorZOffsetCm)
{
    if (!SourceAnchor || !TargetAnchor) return 0;
    const int32 Handle = AddStroke(SourceAnchor->GetActorLocation(), TargetAnchor->GetActorLocation(),
        ThicknessCm, Color, Intensity, Timing);
    // AddStroke just claimed the slot BEHIND the cursor.
    FEffectSlot& Slot = StrokeState[(NextStrokeSlot + StrokeSlots - 1) % StrokeSlots];
    Slot.bAnchored = true;
    Slot.AnchorA = SourceAnchor;
    Slot.AnchorB = TargetAnchor;
    Slot.AnchorZOffsetCm = AnchorZOffsetCm;
    return Handle;
}

int32 ABreakerEffectRenderer::AddBlinkLight(const FVector& Center, float AttenuationRadiusCm,
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
    Slot.Serial = NextSerial++;
    NextLightSlot = (NextLightSlot + 1) % EffectLightSlots;
    return Slot.Serial;
}

void ABreakerEffectRenderer::EndEffect(int32 Handle, float FadeOutSeconds)
{
    if (Handle <= 0) return;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const float Fade = FMath::Max(FadeOutSeconds, 0.0f);
    // The duration rewrite from BreakerEffectMath.h: the clip now ends Fade
    // seconds from this instant. A clip still scheduled for the future is
    // simply killed — it was never seen, so it has nothing to fade from.
    const auto EndSlot = [&](auto& Slot)
    {
        if (!Slot.bActive || Slot.Serial != Handle) return false;
        const float Age = static_cast<float>(Now - Slot.StartTime);
        if (Age < 0.0f)
        {
            Slot.bActive = false;
            return true;
        }
        Slot.Timing.DurationSeconds = FMath::Min(Slot.Timing.DurationSeconds, Age + Fade);
        Slot.Timing.FadeOutSeconds = Fade;
        return true;
    };
    for (FEffectSlot& Slot : GlowState) { if (EndSlot(Slot)) return; }
    for (FEffectSlot& Slot : StrokeState) { if (EndSlot(Slot)) return; }
    for (FEffectLightSlot& Slot : LightState) { if (EndSlot(Slot)) return; }
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

    // Scheduled moments fire first so a fallback glow born this frame is
    // drawn this frame, not next.
    for (FPendingMoment& Pending : PendingMoments)
    {
        if (!Pending.bActive || Now < Pending.FireTime) continue;
        Pending.bActive = false;
        PlayMomentNow(Pending.Moment, Pending.Location, Pending.Direction, Pending.Color);
    }

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
        if (Slot.bAnchored)
        {
            // A beam follows its anchors; an anchor that died ends the beam
            // this frame rather than freezing it where the actor last stood.
            AActor* A = Slot.AnchorA.Get();
            AActor* B = Slot.AnchorB.Get();
            if (!A || !B)
            {
                Slot.bActive = false;
                Hide(Mesh);
                continue;
            }
            const FVector Lift(0.0f, 0.0f, Slot.AnchorZOffsetCm);
            Slot.A = A->GetActorLocation() + Lift;
            Slot.B = B->GetActorLocation() + Lift;
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
