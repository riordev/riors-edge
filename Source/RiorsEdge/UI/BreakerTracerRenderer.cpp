#include "UI/BreakerTracerRenderer.h"

#include "Camera/PlayerCameraManager.h"
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

void ABreakerTracerRenderer::AddTracer(const FVector& Start, const FVector& End)
{
    // Round-robin. Overwriting the oldest slot is the right failure: when
    // twelve rounds really are in the air the one that disappears is the one
    // nearest its own death anyway.
    FTracerSlot& Slot = TracerState[NextTracerSlot];
    Slot.Start = Start;
    Slot.End = End;
    Slot.StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    Slot.bActive = true;
    NextTracerSlot = (NextTracerSlot + 1) % TracerSlots;
}

void ABreakerTracerRenderer::AddImpact(const FVector& Location, bool bWeakPoint, float DelaySeconds)
{
    FSparkSlot& Slot = SparkState[NextSparkSlot];
    Slot.Location = Location;
    Slot.StartTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + FMath::Max(DelaySeconds, 0.0f);
    Slot.bWeakPoint = bWeakPoint;
    Slot.bActive = true;
    NextSparkSlot = (NextSparkSlot + 1) % SparkSlots;
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

        const BreakerHUD::FTracerSample Sample = BreakerHUD::SampleTracer(
            Flight, Slot.Start, Slot.End, static_cast<float>(Now - Slot.StartTime));
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
        const float Thickness = BreakerHUD::TracerThicknessCm(
            Look.ThicknessCm, Distance, Look.MinScreenFraction, VerticalHalfFOV);
        // A round dims very slightly as it goes downrange, so the streak has a
        // direction even in a still frame.
        const float Fade = FMath::Lerp(1.0f, 0.78f, Sample.HeadFraction);

        PlaceSegment(Head, HeadMaterials.IsValidIndex(Index) ? HeadMaterials[Index].Get() : nullptr,
            Sample.HeadStart, Sample.Head, Thickness,
            BreakerUI::Orange, Look.HeadIntensity * Fade);

        if (BreakerHUD::TracerHasTrail(Sample))
        {
            PlaceSegment(Trail, TrailMaterials.IsValidIndex(Index) ? TrailMaterials[Index].Get() : nullptr,
                Sample.Tail, Sample.HeadStart, Thickness * Look.TrailThicknessScale,
                BreakerUI::OrangeDeep, Look.TrailIntensity * Fade);
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
}
