#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/BreakerTracerMath.h"
#include "BreakerTracerRenderer.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// Rounds in flight, IN THE WORLD.
//
// WHY THIS ACTOR EXISTS. The first pass at the tracer drew it on the HUD
// canvas and said so in a comment: a canvas line does not depth-sort, it
// composites over everything, and the fix if that ever showed was to move into
// the world. It showed. A bright stroke that draws in front of the pillar it
// should be behind, with a constant screen-space width, is a decal sliding
// over the screen — the owner's word for it was "weird" and that is the right
// word.
//
// So the round is a real primitive now. Unlit additive translucency still
// depth-TESTS against the opaque scene, so a wall occludes it correctly, and
// because it occupies world space it foreshortens when you shoot along it
// instead of staying the same length at every angle.
//
// POOLED, not spawned. An SMG at 800 RPM fires thirteen rounds a second; a
// spawn/destroy per bullet would be thirteen actor lifecycles a second for a
// thing that lives a fifth of a second. Every primitive is a
// CreateDefaultSubobject on this one actor, allocated once, and slots are
// recycled oldest-first when the pool is full.
//
// This is a CLIENT-SIDE COSMETIC actor. It replicates nothing, resolves
// nothing, and its absence changes no rule — the HUD spawns it lazily on the
// first shot and it is fine for it never to exist.
// ---------------------------------------------------------------------------
UCLASS(NotBlueprintable, NotPlaceable)
class RIORSEDGE_API ABreakerTracerRenderer : public AActor
{
    GENERATED_BODY()

public:
    ABreakerTracerRenderer();

    // One round leaving the barrel. Start is the VISUAL muzzle, End is the
    // impact (or the end of the trace when it hit nothing). The shot itself
    // already resolved; this is a replay.
    void AddTracer(const FVector& Start, const FVector& End);

    // A point flash where a round landed. DelaySeconds is the flight time, so
    // the spark appears when the round ARRIVES rather than when the trigger
    // was pulled — this is called even for rounds that get no visible streak,
    // which is what keeps the hit feedback on every shot while the streaks
    // stay every third one.
    void AddImpact(const FVector& Location, bool bWeakPoint, float DelaySeconds);

    // The knobs. Both are plain (non-reflected) structs of O2 PLACEHOLDER
    // floats — see BreakerTracerMath.h for what each one does. They are not
    // UPROPERTYs because this actor is never placed, never selected and never
    // saved; the tuning surface is the two struct defaults in that header,
    // which the automation tests also read.
    BreakerHUD::FTracerFlight Flight;
    BreakerHUD::FTracerLook Look;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    // Twelve rounds in the air at once. At 800 RPM with one round in three
    // traced and a 0.2 s ceiling on flight time, the true worst case is under
    // one; twelve is headroom for a shotgun-plus-SMG swap storm and still only
    // twenty-four primitives.
    static constexpr int32 TracerSlots = 12;
    // Sparks fire on EVERY hit, not every third, so this pool is the busier
    // one — a shotgun pellet spread plus sustained SMG hits.
    static constexpr int32 SparkSlots = 24;

    struct FTracerSlot
    {
        FVector Start = FVector::ZeroVector;
        FVector End = FVector::ZeroVector;
        double StartTime = 0.0;
        bool bActive = false;
    };

    struct FSparkSlot
    {
        FVector Location = FVector::ZeroVector;
        double StartTime = 0.0;   // Already includes the flight delay.
        bool bWeakPoint = false;
        bool bActive = false;
    };

    // Head and trail are separate primitives because a single stretched box
    // cannot be bright at one end and dim at the other without a material that
    // does not exist yet. Two boxes is the asset-free way to make the round
    // read as pointed.
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> HeadMeshes;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> TrailMeshes;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SparkMeshes;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> HeadMaterials;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> TrailMaterials;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> SparkMaterials;

    FTracerSlot TracerState[TracerSlots];
    FSparkSlot SparkState[SparkSlots];
    int32 NextTracerSlot = 0;
    int32 NextSparkSlot = 0;

    // Camera pose for this frame, resolved once rather than per primitive.
    void ResolveView(FVector& OutLocation, float& OutVerticalHalfFOV) const;
    // Places one unit-cube primitive along a segment. Hidden when the segment
    // is degenerate, which is the normal state of most of the pool.
    void PlaceSegment(UStaticMeshComponent* Mesh, UMaterialInstanceDynamic* Material,
        const FVector& A, const FVector& B, float ThicknessCm,
        const FLinearColor& Color, float Intensity);
    static void Hide(UStaticMeshComponent* Mesh);
};
