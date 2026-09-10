#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreakerPocketRift.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// A SMALL RIFT AT A POCKET — where a returning patrol comes from.
//
// O268 made repopulation fair (an emergence window, a clearance gate, one body
// at a time on a shared clock) and left it ILLEGIBLE: bodies resolved into
// existence standing on their posts. The courtyard already has the one
// authored doorway in the yard; the other five pockets have nothing to arrive
// out of, and the desk's own note says authoring mouths across the yard is a
// composer job rather than a code one. So the arrival point is not a hole in a
// wall — it is a tear, which needs no geometry, works on any ground, and says
// what Fernhall actually is.
//
// IT IS A COSMETIC ACTOR AND IT RESOLVES NOTHING. The clearance gate, the
// emergence window and the walk to the post are all the game mode's, exactly
// as they were; this draws where that already happens. Its absence changes no
// rule, which is the same contract ABreakerEffectRenderer keeps.
//
// NOT BUILT ON THE EFFECT POOL, deliberately. ABreakerEffectRenderer's slots
// are recycled oldest-first for ability bursts; five permanent tears would hold
// thirty of them for the life of the level and starve the thing the pool exists
// for. It owns its own meshes instead, as the Volatile blast ring does.
//
// AND NOT ON AN INSTANCED COMPONENT, WHICH IS A MEASURED FINDING RATHER THAN A
// PREFERENCE. The first version was one UInstancedStaticMeshComponent — the
// blast ring's shape exactly — and it photographed SOLID BLACK. A material must
// declare MATUSAGE_InstancedStaticMeshes to draw on an instanced component, and
// the probe says /Engine/EngineMaterials/EmissiveMeshMaterial (the additive
// glow every tracer and beacon in this project uses) does NOT, while
// /Engine/BasicShapes/BasicShapeMaterial (which the blast ring uses, and which
// renders orange correctly) DOES. So the choice is instancing OR additive light,
// and for a tear it has to be light: this is a hole in the world, and a lit
// surface would go grey in a building's shadow and read as painted geometry.
// Twenty-seven small mesh components is what that costs.
// ---------------------------------------------------------------------------
UCLASS(NotBlueprintable, NotPlaceable)
class RIORSEDGE_API ABreakerPocketRift : public AActor
{
    GENERATED_BODY()

public:
    ABreakerPocketRift();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // SOMETHING JUST CAME THROUGH. Called by the repopulation clock at the
    // moment it spawns a body here. Re-flaring a live flare restarts it rather
    // than stacking: two bodies through one tear is two arrivals, not a
    // brighter one.
    void Flare();

    // For the shipped-configuration test: whether the tear actually built its
    // segments, which is the one thing that can silently fail (the engine cube
    // is content, and content can be missing from a cooked build).
    int32 SegmentCount() const;

    // ---- The dials, all O2 PLACEHOLDER until the owner has stood in front of
    // one. Sized against a body rather than by eye: an enemy capsule is about
    // 176 cm tall, so a 280 cm tear is a head taller than what walks out of it.
    static constexpr float TearHeightCm = 280.0f;
    static constexpr float TearWidthCm = 150.0f;
    // Deliberately coarse. A smooth lens reads as a designed shape; a faceted
    // one reads as something torn.
    static constexpr int32 TearSteps = 12;
    static constexpr float SegmentThicknessCm = 9.0f;
    static constexpr float SegmentDashFraction = 0.72f;
    // How far the edge wanders, as a fraction of the local half-width. Zero
    // draws the clean lens the first capture rejected as an emblem.
    static constexpr float EdgeWander = 0.30f;
    static constexpr float IdleHz = 0.35f;
    static constexpr float IdleAmplitude = 0.035f;
    static constexpr float IdleGlow = 1.6f;
    static constexpr float IdleLightIntensity = 900.0f;
    static constexpr float FlareSeconds = 1.6f;
    static constexpr float FlareWiden = 0.30f;
    static constexpr float FlareGlow = 5.0f;
    static constexpr float FlareLightIntensity = 5200.0f;
    static constexpr float LightAttenuationCm = 1100.0f;

private:
    UPROPERTY() TObjectPtr<USceneComponent> Pivot;
    // The thing that BREATHES. Every segment hangs off it, so the pulse is one
    // transform rather than twenty-seven.
    UPROPERTY() TObjectPtr<USceneComponent> TearRoot;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Segments;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> TearMaterial;
    UPROPERTY() TObjectPtr<UPointLightComponent> Bloom;

    float Age = 0.0f;
    float FlareRemaining = 0.0f;
};
