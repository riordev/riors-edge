#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/BreakerEffectMath.h"
#include "BreakerEffectRenderer.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// Ability visuals, IN THE WORLD. The tracer renderer's sibling.
//
// Thirty-two abilities are implemented and none of them draws anything. This
// actor is the machinery that changes that, built on the four decisions the
// tracer already paid for the lessons on:
//
//  * POOLED PRIMITIVES, allocated once in the constructor, recycled
//    oldest-first — an ability storm must never be an actor-lifecycle storm.
//  * POOLED SHADOWLESS LIGHTS, far fewer than meshes, because a dynamic
//    light is the expensive primitive and "the room noticed" only needs a
//    handful at once.
//  * WORLD-PLACED unlit-additive surfaces (BreakerGlowMaterial), so the
//    depth buffer occludes an effect behind a pillar. The first tracer drew
//    on the HUD canvas and the owner's word for the result was "weird";
//    that mistake is not available to this file.
//  * TG_PostUpdateWork, so nothing shears against last frame's camera.
//
// The VOCABULARY is deliberately primitive: a glow (sphere) and a stroke
// (segment), each living on a BreakerFX::FEffectTiming clock, plus a blink
// light. Ability shapes — Rot's ground ring, Siphon's beam, Unmake's chain —
// are Phase C compositions OF these calls, with their geometry read from the
// ability's own header, never invented here. Siphon's channel-break is a
// duration rewrite on its slots (see BreakerEffectMath.h), which will need
// this actor to hand out slot handles; that seam is Phase C's first cut.
//
// Like its sibling this is a CLIENT-SIDE COSMETIC actor: replicates nothing,
// resolves nothing, its absence changes no rule, and it is fine for it never
// to exist.
// ---------------------------------------------------------------------------
UCLASS(NotBlueprintable, NotPlaceable)
class RIORSEDGE_API ABreakerEffectRenderer : public AActor
{
    GENERATED_BODY()

public:
    ABreakerEffectRenderer();

    // A sphere of light at a point: bursts, arrival/departure flashes, orbs.
    // DelaySeconds > 0 schedules the birth into the future.
    void AddGlow(const FVector& Center, float RadiusCm, const FLinearColor& Color,
        float Intensity, const BreakerFX::FEffectTiming& Timing, float DelaySeconds = 0.0f);

    // A straight bright segment: beams, chain legs, ring sides, arc strokes.
    void AddStroke(const FVector& Start, const FVector& End, float ThicknessCm,
        const FLinearColor& Color, float Intensity, const BreakerFX::FEffectTiming& Timing,
        float DelaySeconds = 0.0f);

    // A shadowless point light on the same clock, so a burst can make its
    // surroundings answer. Budgeted far below the meshes on purpose.
    void AddBlinkLight(const FVector& Center, float AttenuationRadiusCm,
        const FLinearColor& Color, float Intensity, const BreakerFX::FEffectTiming& Timing,
        float DelaySeconds = 0.0f);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    // Pool sizes. All O2 PLACEHOLDER, sized against Phase C's known worst
    // cases: a ground ring is ~16 strokes and two Rot zones plus a chain must
    // coexist, so strokes are the deep pool; glows are events, not shapes;
    // lights follow the tracer's "the newest wins" argument at ability rate.
    static constexpr int32 GlowSlots = 16;
    static constexpr int32 StrokeSlots = 48;
    static constexpr int32 EffectLightSlots = 4;

public:
    // Public for the suite's pool-arithmetic assertions, like the tracer's.
    static constexpr int32 GetGlowSlots() { return GlowSlots; }
    static constexpr int32 GetStrokeSlots() { return StrokeSlots; }
    static constexpr int32 GetEffectLightSlots() { return EffectLightSlots; }

private:
    struct FEffectSlot
    {
        // Glow: A is the centre, B unused. Stroke: A to B.
        FVector A = FVector::ZeroVector;
        FVector B = FVector::ZeroVector;
        double StartTime = 0.0;   // Delay already folded in; may be future.
        BreakerFX::FEffectTiming Timing;
        FLinearColor Color = FLinearColor::White;
        // Glow: radius. Stroke: thickness.
        float SizeCm = 0.0f;
        float Intensity = 0.0f;
        bool bActive = false;
    };

    struct FEffectLightSlot
    {
        FVector Center = FVector::ZeroVector;
        double StartTime = 0.0;
        BreakerFX::FEffectTiming Timing;
        FLinearColor Color = FLinearColor::White;
        float AttenuationRadiusCm = 0.0f;
        float Intensity = 0.0f;
        bool bActive = false;
    };

    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> GlowMeshes;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> StrokeMeshes;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMaterials;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> StrokeMaterials;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> EffectLights;

    FEffectSlot GlowState[GlowSlots];
    FEffectSlot StrokeState[StrokeSlots];
    FEffectLightSlot LightState[EffectLightSlots];
    int32 NextGlowSlot = 0;
    int32 NextStrokeSlot = 0;
    int32 NextLightSlot = 0;

    static void Hide(UStaticMeshComponent* Mesh);
};
