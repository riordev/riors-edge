#include "UI/BreakerTracerRenderer.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/BreakerGlowMaterial.h"
#include "UI/BreakerUIStyle.h"

namespace
{
    // Both stock primitives are 100 cm across their bounding box, so a unit of
    // scale is a metre and every dimension below divides by this once.
    constexpr float UnitMeshCm = 100.0f;
    // Prefixed, not bare. An anonymous namespace is per translation unit,
    // but a unity build concatenates several .cpp files INTO one TU, so a
    // bare ShapeCube here collides with the identical constant in
    // BreakerRocketProjectile.cpp. Neither file's own build catches it,
    // because an adaptive non-unity build excludes exactly the files being
    // edited — it only appears when the whole module compiles together.
    const TCHAR* TracerShapeCube = TEXT("/Engine/BasicShapes/Cube.Cube");
    const TCHAR* TracerShapeSphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");

    UStaticMeshComponent* MakePooledMesh(AActor* Owner, USceneComponent* Parent,
        const FString& Name, const TCHAR* MeshPath)
    {
        UStaticMeshComponent* Mesh = Owner->CreateDefaultSubobject<UStaticMeshComponent>(*Name);
        if (!Mesh) return nullptr;
        Mesh->SetupAttachment(Parent);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetGenerateOverlapEvents(false);
        Mesh->SetCastShadow(false);
        // A tracer casting or receiving anything is a bug: it is light, not a
        // surface. Decals and depth-writing translucency both cost frames for
        // a primitive that lives 0.2 s.
        Mesh->bReceivesDecals = false;
        Mesh->SetHiddenInGame(true);
        // Every slot is positioned in world space; inheriting the (stationary)
        // actor transform would just be an extra concatenation per frame.
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

ABreakerTracerRenderer::ABreakerTracerRenderer()
{
    PrimaryActorTick.bCanEverTick = true;
    // Rounds must be placed AFTER the camera has moved for the frame,
    // otherwise a streak fired this frame is drawn against last frame's view
    // and shears sideways when the player turns.
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    bReplicates = false;
    SetReplicatingMovement(false);

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    HeadMeshes.Reserve(TracerSlots);
    TrailMeshes.Reserve(TracerSlots);
    for (int32 Index = 0; Index < TracerSlots; ++Index)
    {
        HeadMeshes.Add(MakePooledMesh(this, Root, FString::Printf(TEXT("TracerHead%d"), Index), TracerShapeCube));
        TrailMeshes.Add(MakePooledMesh(this, Root, FString::Printf(TEXT("TracerTrail%d"), Index), TracerShapeCube));
    }

    SparkMeshes.Reserve(SparkSlots);
    for (int32 Index = 0; Index < SparkSlots; ++Index)
    {
        SparkMeshes.Add(MakePooledMesh(this, Root, FString::Printf(TEXT("Spark%d"), Index), TracerShapeSphere));
    }

    // The blink lights. Shadowless and short-range: the job is "the wall
    // noticed", not "the room is lit". Pooled for the same reason as
    // everything else in this actor.
    ImpactLights.Reserve(ImpactLightSlots);
    for (int32 Index = 0; Index < ImpactLightSlots; ++Index)
    {
        UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(
            *FString::Printf(TEXT("ImpactLight%d"), Index));
        if (Light)
        {
            Light->SetupAttachment(Root);
            Light->SetIntensity(0.0f);
            Light->SetCastShadows(false);
            Light->SetAttenuationRadius(Look.ImpactLightRadiusCm);
            Light->SetVisibility(false);
            Light->SetUsingAbsoluteLocation(true);
        }
        ImpactLights.Add(Light);
    }
}

void ABreakerTracerRenderer::BeginPlay()
{
    Super::BeginPlay();

    // Dynamic instances cannot be created in the constructor, so the whole
    // pool's materials are built once here. If the engine content is missing
    // the arrays stay null and every draw below no-ops rather than crashing.
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

    BuildMaterials(HeadMeshes, HeadMaterials);
    BuildMaterials(TrailMeshes, TrailMaterials);
    BuildMaterials(SparkMeshes, SparkMaterials);
}

void ABreakerTracerRenderer::ClaimTracerSlot(const FVector& Start, const FVector& End, float ThicknessScale,
    const FLinearColor& HeadColor, const FLinearColor& TrailColor, float DelaySeconds)
{
    // Round-robin. Overwriting the oldest slot is the right failure: when
    // twelve rounds really are in the air the one that disappears is the one
    // nearest its own death anyway.
    FTracerSlot& Slot = TracerState[NextTracerSlot];
    Slot.Start = Start;
    Slot.End = End;
    Slot.StartTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + FMath::Max(DelaySeconds, 0.0f);
    Slot.bActive = true;
    Slot.ThicknessScale = ThicknessScale;
    Slot.HeadColor = HeadColor;
    Slot.TrailColor = TrailColor;
    NextTracerSlot = (NextTracerSlot + 1) % TracerSlots;
}

void ABreakerTracerRenderer::AddTracer(const FVector& Start, const FVector& End)
{
    ClaimTracerSlot(Start, End, 1.0f, BreakerUI::Orange, BreakerUI::OrangeDeep);
}

void ABreakerTracerRenderer::AddSecondaryLeg(const FVector& Start, const FVector& End, bool bHit, float DelaySeconds)
{
    // Cyan, the player/system token: this is the build acting, not the gun.
    // The leg flies like a round (SampleTracer replays it from ITS OWN start),
    // so a chain arc visibly leaves the enemy it chained from.
    ClaimTracerSlot(Start, End, SecondaryThicknessScale,
        BreakerUI::Cyan, BreakerUI::Cyan * 0.35f, DelaySeconds);
    if (bHit)
    {
        const float LegFlight = BreakerHUD::TracerFlightSeconds(
            Flight, static_cast<float>((End - Start).Size()));
        // Secondary hits are never weak points today (the leg struct carries
        // no flag); the ordinary orange spark plus the cyan streak is already
        // a distinct signature.
        AddImpact(End, false, DelaySeconds + LegFlight);
    }
}

int32 ABreakerTracerRenderer::AddSpread(const FVector& Start, TArrayView<const FBreakerPelletImpact> Pellets)
{
    const int32 PelletCount = Pellets.Num();
    if (PelletCount <= 0) return 0;

    // --- Streaks: a budgeted, thinner subsample -----------------------------
    // See the pool-sharing note in the header. The budget is what guarantees
    // this can never wrap the pool and evict its own earlier streaks, which is
    // the difference between "shares the pool" and "silently drops pellets".
    const int32 StreakCount = BreakerHUD::SpreadStreakCount(PelletCount, MaxSpreadStreaks);
    for (int32 StreakIndex = 0; StreakIndex < StreakCount; ++StreakIndex)
    {
        const int32 PelletIndex = BreakerHUD::SpreadStreakPellet(StreakIndex, StreakCount, PelletCount);
        ClaimTracerSlot(Start, Pellets[PelletIndex].End, SpreadThicknessScale,
            BreakerUI::Orange, BreakerUI::OrangeDeep);
    }

    // --- Flashes: every landed pellet, up to the spark budget ---------------
    // Hit confirmation is feedback the player acts on, so this is deliberately
    // NOT the same subsample as the streaks: it follows the pellets that
    // actually landed. It is still budgeted, because a 32-pellet definition
    // would otherwise wrap the 24-slot spark pool inside one trigger pull.
    int32 Flashes = 0;
    for (const FBreakerPelletImpact& Pellet : Pellets)
    {
        if (!Pellet.bHit) continue;
        if (Flashes >= MaxSpreadSparks) break;
        const float FlightSeconds = BreakerHUD::TracerFlightSeconds(
            Flight, static_cast<float>((Pellet.End - Start).Size()));
        AddImpact(Pellet.End, Pellet.bWeakPoint, FlightSeconds);
        ++Flashes;
    }
    return StreakCount;
}

void ABreakerTracerRenderer::AddImpact(const FVector& Location, bool bWeakPoint, float DelaySeconds)
{
    const double ArrivalTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + FMath::Max(DelaySeconds, 0.0f);

    FSparkSlot& Slot = SparkState[NextSparkSlot];
    Slot.Location = Location;
    Slot.StartTime = ArrivalTime;
    Slot.bWeakPoint = bWeakPoint;
    Slot.bActive = true;
    NextSparkSlot = (NextSparkSlot + 1) % SparkSlots;

    // Every spark also claims a blink light. The light pool is smaller on
    // purpose: under a shotgun blast the last six pellets keep their glow and
    // the rest keep only the emissive spark, which still reads as one blast.
    // S2 NOTE (unowned domain): the per-impact tick SOUND would be scheduled
    // off this same arrival time — noted here, not built.
    FImpactLightSlot& LightSlot = ImpactLightState[NextImpactLightSlot];
    LightSlot.Location = Location;
    LightSlot.StartTime = ArrivalTime;
    LightSlot.bWeakPoint = bWeakPoint;
    LightSlot.bActive = true;
    NextImpactLightSlot = (NextImpactLightSlot + 1) % ImpactLightSlots;
}

void ABreakerTracerRenderer::ResolveView(FVector& OutLocation, float& OutVerticalHalfFOV) const
{
    OutLocation = FVector::ZeroVector;
    OutVerticalHalfFOV = FMath::DegreesToRadians(35.0f);

    const UWorld* World = GetWorld();
    const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    const APlayerCameraManager* Camera = PC ? PC->PlayerCameraManager : nullptr;
    if (!Camera) return;

    OutLocation = Camera->GetCameraLocation();
    // The camera reports a HORIZONTAL field of view; the screen-width floor is
    // expressed against viewport HEIGHT, so it converts through the aspect.
    const float HorizontalHalf = FMath::DegreesToRadians(FMath::Clamp(Camera->GetFOVAngle(), 20.0f, 170.0f) * 0.5f);
    float Aspect = 0.5625f;
    const float CachedAspect = Camera->GetCameraCacheView().AspectRatio;
    if (CachedAspect > KINDA_SMALL_NUMBER) Aspect = 1.0f / CachedAspect;
    OutVerticalHalfFOV = FMath::Atan(FMath::Tan(HorizontalHalf) * Aspect);
}

void ABreakerTracerRenderer::Hide(UStaticMeshComponent* Mesh)
{
    if (Mesh && !Mesh->bHiddenInGame) Mesh->SetHiddenInGame(true);
}

void ABreakerTracerRenderer::PlaceSegment(UStaticMeshComponent* Mesh, UMaterialInstanceDynamic* Material,
    const FVector& A, const FVector& B, float ThicknessCm, const FLinearColor& Color, float Intensity)
{
    if (!Mesh) return;
    const FVector Delta = B - A;
    const float Length = static_cast<float>(Delta.Size());
    if (Length < 1.0f)
    {
        Hide(Mesh);
        return;
    }

    Mesh->SetWorldLocationAndRotation((A + B) * 0.5,
        FRotationMatrix::MakeFromX(Delta / Length).Rotator());
    Mesh->SetWorldScale3D(FVector(Length / UnitMeshCm,
        ThicknessCm / UnitMeshCm, ThicknessCm / UnitMeshCm));
    BreakerUI::SetGlowColor(Material, Color, Intensity);
    if (Mesh->bHiddenInGame) Mesh->SetHiddenInGame(false);
}

void ABreakerTracerRenderer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    const UWorld* World = GetWorld();
    if (!World) return;
    const double Now = World->GetTimeSeconds();

    FVector ViewLocation;
    float VerticalHalfFOV;
    ResolveView(ViewLocation, VerticalHalfFOV);

    for (int32 Index = 0; Index < TracerSlots; ++Index)
    {
        FTracerSlot& Slot = TracerState[Index];
        UStaticMeshComponent* Head = HeadMeshes.IsValidIndex(Index) ? HeadMeshes[Index].Get() : nullptr;
        UStaticMeshComponent* Trail = TrailMeshes.IsValidIndex(Index) ? TrailMeshes[Index].Get() : nullptr;
        if (!Slot.bActive)
        {
            Hide(Head);
            Hide(Trail);
            continue;
        }

        const float Age = static_cast<float>(Now - Slot.StartTime);
        if (Age < 0.0f)
        {
            // A secondary leg scheduled for the future: hidden, not finished.
            Hide(Head);
            Hide(Trail);
            continue;
        }

        const BreakerHUD::FTracerSample Sample = BreakerHUD::SampleTracer(
            Flight, Slot.Start, Slot.End, Age);
        if (!Sample.bVisible)
        {
            // Arrived, or never had room to be a streak. Either way the slot
            // is finished: the impact spark is scheduled independently.
            Slot.bActive = false;
            Hide(Head);
            Hide(Trail);
            continue;
        }

        // Thickness is resolved at the HEAD's distance and used for the whole
        // streak. Tapering it along its own length would be more correct and
        // would also make a 2.4 m primitive visibly wedge-shaped, which reads
        // as a cone rather than as a round.
        const float Distance = static_cast<float>(FVector::Dist(ViewLocation, Sample.Head));
        // The per-slot scale is applied to the AUTHORED thickness and not to
        // the result, so the screen-width floor still holds: a spread pellet at
        // 40 m is thinner than a bullet in world terms but is still at least a
        // pixel wide, which is what stops the far half of a cone strobing out
        // of existence between frames.
        const float Thickness = BreakerHUD::TracerThicknessCm(
            Look.ThicknessCm * Slot.ThicknessScale, Distance, Look.MinScreenFraction, VerticalHalfFOV);
        // A round dims very slightly as it goes downrange, so the streak has a
        // direction even in a still frame.
        const float Fade = FMath::Lerp(1.0f, 0.78f, Sample.HeadFraction);

        PlaceSegment(Head, HeadMaterials.IsValidIndex(Index) ? HeadMaterials[Index].Get() : nullptr,
            Sample.HeadStart, Sample.Head, Thickness,
            Slot.HeadColor, Look.HeadIntensity * Fade);

        if (BreakerHUD::TracerHasTrail(Sample))
        {
            PlaceSegment(Trail, TrailMaterials.IsValidIndex(Index) ? TrailMaterials[Index].Get() : nullptr,
                Sample.Tail, Sample.HeadStart, Thickness * Look.TrailThicknessScale,
                Slot.TrailColor, Look.TrailIntensity * Fade);
        }
        else
        {
            Hide(Trail);
        }
    }

    for (int32 Index = 0; Index < SparkSlots; ++Index)
    {
        FSparkSlot& Slot = SparkState[Index];
        UStaticMeshComponent* Mesh = SparkMeshes.IsValidIndex(Index) ? SparkMeshes[Index].Get() : nullptr;
        if (!Slot.bActive)
        {
            Hide(Mesh);
            continue;
        }

        const float Age = static_cast<float>(Now - Slot.StartTime);
        if (Age < 0.0f)
        {
            // Scheduled but the round has not landed yet.
            Hide(Mesh);
            continue;
        }
        if (Age >= Look.ImpactSeconds)
        {
            Slot.bActive = false;
            Hide(Mesh);
            continue;
        }

        // Pops to full size instantly and collapses: a spark is the decay, not
        // the growth. The old star expanded outward, which is what a shockwave
        // does, not what a bullet strike does.
        const float Progress = Age / FMath::Max(Look.ImpactSeconds, KINDA_SMALL_NUMBER);
        const float Radius = Look.ImpactRadiusCm * (1.0f - FMath::Square(Progress));
        if (Radius < 0.5f || !Mesh)
        {
            Hide(Mesh);
            continue;
        }

        // Weak points keep the gold they already own everywhere else in the
        // HUD; an ordinary body shot is the weapon/heat orange.
        const FLinearColor Color = Slot.bWeakPoint ? BreakerUI::Gold : BreakerUI::Orange;
        Mesh->SetWorldLocation(Slot.Location);
        Mesh->SetWorldScale3D(FVector(Radius * 2.0f / UnitMeshCm));
        BreakerUI::SetGlowColor(
            SparkMaterials.IsValidIndex(Index) ? SparkMaterials[Index].Get() : nullptr,
            Color, Look.ImpactIntensity * (1.0f - Progress));
        if (Mesh->bHiddenInGame) Mesh->SetHiddenInGame(false);
    }

    // The blink lights: pop to full the instant the round arrives, decay as a
    // square so most of the light is in the first frames — a blink, not a lamp.
    for (int32 Index = 0; Index < ImpactLightSlots; ++Index)
    {
        FImpactLightSlot& Slot = ImpactLightState[Index];
        UPointLightComponent* Light = ImpactLights.IsValidIndex(Index) ? ImpactLights[Index].Get() : nullptr;
        if (!Light) continue;
        if (!Slot.bActive)
        {
            if (Light->IsVisible()) Light->SetVisibility(false);
            continue;
        }

        const float Age = static_cast<float>(Now - Slot.StartTime);
        if (Age < 0.0f)
        {
            if (Light->IsVisible()) Light->SetVisibility(false);
            continue;
        }
        if (Age >= Look.ImpactLightSeconds)
        {
            Slot.bActive = false;
            Light->SetVisibility(false);
            continue;
        }

        const float Progress = Age / FMath::Max(Look.ImpactLightSeconds, KINDA_SMALL_NUMBER);
        const float Falloff = FMath::Square(1.0f - Progress);
        // Weak points blink in their gold and noticeably harder — the loudest
        // frame of a weak-point hit is the light, not the spark.
        Light->SetWorldLocation(Slot.Location);
        Light->SetLightColor(Slot.bWeakPoint ? BreakerUI::Gold : BreakerUI::Orange);
        Light->SetIntensity(Look.ImpactLightIntensity
            * (Slot.bWeakPoint ? Look.WeakPointLightScale : 1.0f) * Falloff);
        if (!Light->IsVisible()) Light->SetVisibility(true);
    }
}
