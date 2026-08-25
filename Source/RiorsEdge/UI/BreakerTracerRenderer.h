#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/BreakerTracerMath.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "BreakerTracerRenderer.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// How a SPREAD shares a fixed pool.
//
// The pool is twelve tracer slots, allocated once, recycled oldest-first. A
// shotgun fires eight pellets and a definition may author up to thirty-two, so
// "one streak per pellet" would consume two thirds of the pool in a single
// trigger pull and a second blast would evict the first one mid-flight. That is
// the exact failure the round-robin was written to make graceful for SINGLE
// rounds and is NOT graceful for a spread: half a cone disappearing is worse
// than no cone.
//
// The policy, therefore: a spread gets a BUDGET of slots and a SUBSAMPLE of its
// pellets, drawn thinner. Three separate reasons, in order of weight:
//
//  1. It bounds the pool. `MaxSpreadStreaks` slots per spread out of twelve
//     means at least two whole spreads can be in the air at once and no spread
//     can ever evict itself. At 85 RPM (0.7 s between shells) against a 0.20 s
//     flight ceiling, two is already unreachable in practice.
//  2. It is honest about what a shotgun looks like. The information a player
//     needs from a blast is WHERE THE CONE WENT — its width and its centre —
//     not the individual trajectory of pellet six. Sampling the extremes and
//     the middle carries that; drawing all eight is a solid wedge of light that
//     carries less.
//  3. Eight full-thickness streaks read as eight rifle rounds fired at once.
//     `SpreadThicknessScale` makes each sub-streak thinner than a bullet, so
//     the group reads as shot rather than as a volley.
//
// The subsample always includes the FIRST and LAST pellet of the spread, so the
// drawn cone is exactly as wide as the real one and never narrower.
//
// Pure, header-inline, no world: the automation suite calls these directly,
// because whether the pool overflows is arithmetic and is therefore the one
// part of this that a test can actually prove.
// ---------------------------------------------------------------------------
namespace BreakerHUD
{
    // How many streaks a spread of PelletCount pellets is allowed to draw.
    inline int32 SpreadStreakCount(int32 PelletCount, int32 MaxStreaks)
    {
        if (PelletCount <= 0 || MaxStreaks <= 0) return 0;
        return FMath::Min(PelletCount, MaxStreaks);
    }

    // Which pellet the StreakIndex'th streak draws. Evenly spaced across the
    // spread and inclusive of both ends, so streak 0 is always pellet 0 and the
    // last streak is always the last pellet.
    inline int32 SpreadStreakPellet(int32 StreakIndex, int32 StreakCount, int32 PelletCount)
    {
        if (PelletCount <= 0 || StreakCount <= 0) return 0;
        if (StreakCount == 1) return 0;
        const int32 Clamped = FMath::Clamp(StreakIndex, 0, StreakCount - 1);
        return FMath::RoundToInt(
            static_cast<float>(Clamped) * static_cast<float>(PelletCount - 1)
            / static_cast<float>(StreakCount - 1));
    }
}

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

    // A whole spread from one trigger pull, streaks and impact flashes
    // together. Pellets are the shot contract's per-pellet record, in fire
    // order, hits and misses alike — a missed pellet still gets a streak,
    // because a cone with only its hits drawn is a lie about where you shot.
    //
    // Budgeted: see the pool-sharing note above. Returns how many streaks were
    // actually drawn, which is what the automation suite asserts against the
    // pool size.
    int32 AddSpread(const FVector& Start, TArrayView<const FBreakerPelletImpact> Pellets);

    // One SECONDARY leg of a shot: a pierce continuation, a chain arc or a
    // ricochet bounce (FBreakerShotResult::SecondaryImpacts). Drawn in its own
    // tint so the Swift identity is SEEABLE — a pierce that looks identical to
    // the round that caused it is invisible content. DelaySeconds is how long
    // after the trigger pull this leg begins (the primary round's flight, plus
    // a beat per leg), so an arc visibly CONTINUES from the hit rather than
    // appearing alongside it. Schedules its own impact spark when the leg
    // landed.
    void AddSecondaryLeg(const FVector& Start, const FVector& End, bool bHit, float DelaySeconds);

    // Legs one trigger pull may draw. Six of twelve slots: a deep
    // pierce+chain build fires legs every shot, and the cap is what keeps a
    // multishot Swift from evicting its own primary streaks. O2 PLACEHOLDER
    static constexpr int32 MaxSecondaryLegStreaks = 6;
    // Legs are slightly thinner than the round that spawned them — the
    // continuation, not the shot. O2 PLACEHOLDER
    static constexpr float SecondaryThicknessScale = 0.8f;

    // Slots one spread may occupy, out of TracerSlots. Four of twelve leaves
    // room for two more spreads plus a swap to another weapon mid-flight.
    // O2 PLACEHOLDER
    static constexpr int32 MaxSpreadStreaks = 4;
    // Landed pellets one spread may flash, out of SparkSlots. Eight of
    // twenty-four is a full shotgun blast with room for two more.
    // O2 PLACEHOLDER
    static constexpr int32 MaxSpreadSparks = 8;
    // Sub-streaks are thinner than a bullet so a group reads as shot rather
    // than as a volley of rifle rounds. O2 PLACEHOLDER
    static constexpr float SpreadThicknessScale = 0.55f;

    // A point flash where a round landed. DelaySeconds is the flight time, so
    // the spark appears when the round ARRIVES rather than when the trigger
    // was pulled — this is called even for rounds that get no visible streak,
    // which is what keeps the hit feedback on every shot while the streaks
    // stay every third one.
    void AddImpact(const FVector& Location, bool bWeakPoint, float DelaySeconds);

    // The knobs. Both are plain (non-reflected) structs of O2 PLACEHOLDER
    // floats — see BreakerTracerMath.h for what each one does. They are not
    // UPROPERTYs because this actor is never placed, never selected and never
    // saved. THE TUNING SURFACE IS THE Breaker.Tracer.* CONSOLE VARIABLES:
    // these members are refreshed from BreakerHUD::LiveTracerFlight()/
    // LiveTracerLook() every Tick, so a console change lands next frame.
    // The struct defaults in the header remain the authored values the
    // automation tests read.
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

public:
    // Public so the automation suite can assert that the spread budget cannot
    // overrun the pool. Nothing else reads them.
    static constexpr int32 GetTracerSlots() { return TracerSlots; }
    static constexpr int32 GetSparkSlots();

private:
    // Sparks fire on EVERY hit, not every third, so this pool is the busier
    // one — a shotgun pellet spread plus sustained SMG hits.
    static constexpr int32 SparkSlots = 24;

    struct FTracerSlot
    {
        FVector Start = FVector::ZeroVector;
        FVector End = FVector::ZeroVector;
        // May be in the FUTURE: a secondary leg schedules itself to begin when
        // the round that spawned it arrives. A slot whose time has not come
        // yet is hidden, not recycled.
        double StartTime = 0.0;
        bool bActive = false;
        // 1.0 for an ordinary round; SpreadThicknessScale for one sub-streak
        // of a spread. Held per slot rather than read at draw time because the
        // slot outlives the call that filled it.
        float ThicknessScale = 1.0f;
        // Primary rounds are the weapon orange; secondary legs carry the
        // player-system cyan so pierce/chain/ricochet read as manipulation
        // rather than as more bullets. Held per slot, same reason as above.
        FLinearColor HeadColor = FLinearColor::White;
        FLinearColor TrailColor = FLinearColor::White;
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

    // Impact BLINK lights: a small pool of point lights that pop where a round
    // landed, so the surface around the hit answers it. Far fewer than sparks
    // because a dynamic light is the expensive primitive in this file — when
    // more hits land than lights exist, the newest hits win, which is where
    // the player is looking anyway.
    static constexpr int32 ImpactLightSlots = 6;
    struct FImpactLightSlot
    {
        FVector Location = FVector::ZeroVector;
        double StartTime = 0.0;   // Already includes the flight delay.
        bool bWeakPoint = false;
        bool bActive = false;
    };
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> ImpactLights;
    FImpactLightSlot ImpactLightState[ImpactLightSlots];
    int32 NextImpactLightSlot = 0;

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
    // The one place a tracer slot is claimed, so the round-robin cursor and the
    // per-slot state can never disagree. DelaySeconds > 0 schedules the streak
    // to begin in the future (secondary legs).
    void ClaimTracerSlot(const FVector& Start, const FVector& End, float ThicknessScale,
        const FLinearColor& HeadColor, const FLinearColor& TrailColor, float DelaySeconds = 0.0f);
};

constexpr int32 ABreakerTracerRenderer::GetSparkSlots() { return SparkSlots; }
