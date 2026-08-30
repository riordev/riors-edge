#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BreakerBodyPaint.h"
#include "BreakerHitReactionComponent.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// THE REACTION LAYER, AS A COMPONENT ANYTHING SHOOTABLE CAN CARRY, AND THE
// ONE WRITER OF THE BODY'S `Color` (O128).
//
// Every flinch, hit flash and death crumple in the game lived on
// ABreakerEnemy, and the gym's target dummy is a bare actor — which is the
// whole explanation for the dead gym: the feedback existed and fired only
// on things the gym doesn't spawn. Ruled: extract, don't promote — a dummy
// that inherited ABreakerEnemy to get a flash would stop being a dummy.
//
// WHAT CHANGED WITH O128: this component no longer OWNS a flash on top of
// somebody else's paint. It owns the body's colour outright. Every layer —
// the family paint, the rank blend, the health ramp, the reaction — is a
// field of one BreakerBodyPaint::FState, and every write recomputes the
// whole colour from that state through a pure Resolve. There is no capture,
// no restore, and no GetVectorParameterValue anywhere on this path. The two
// caches this replaces (this component's BodyMaterialBases and the enemy's
// RankBaseColors) were two snapshots of one parameter, and the ordering
// between them was a live race; see BreakerBodyPaint.h for the failure.
//
// What this owns:
//  * the BODY PAINT — layers pushed in by the owner, resolved and written to
//    every registered part whenever any of them moves;
//  * the HIT FLASH — a one-blink pulse, gold on a weak point, which now
//    simply sets a state field and lets the resolver occlude;
//  * the DEATH PRESENTATION — beat one THE POP (whole-actor scale swell in
//    the flash colour, harder and gold on a weak-point kill), beat two THE
//    CRUMPLE (squash to the ground, colour burning down to ash);
//  * the revive restore (Wakeful, and the pool) — scale returned, and the
//    resting colour recomputed rather than restored, so a revive gives back
//    exactly what the layers say it should whatever was in flight.
//
// What it deliberately does NOT own: visibility (the owner hides its own
// body on OnDeathPresentationFinished — an enemy and a dummy hide different
// sets of things), vitals, and any rule at all. Cosmetic only; nothing may
// ever read it back into gameplay.
// ---------------------------------------------------------------------------
UCLASS(ClassGroup=(Breaker), meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerHitReactionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerHitReactionComponent();

    // The meshes this paints. Registered once the owner's parts are final
    // (BeginPlay); order is irrelevant and null parts are skipped.
    // Registration repaints, so a part joining late lands on the current
    // resolved colour rather than whatever its constructor left.
    void RegisterPart(UStaticMeshComponent* Part);

    // A NAMED body (the mech cast) keeps its livery and wears the paint as an
    // OVERLAY: the same resolved colour, at BreakerBodyPaint's overlay
    // strength — zero at rest, occluding through reactions. Same state, same
    // one writer; the overlay material asset resolves lazily and a clone
    // without it keeps a paintless (but complete) body.
    void RegisterOverlayBody(class UMeshComponent* Body);

    // --- The layers. Each setter repaints; none reads anything back. ------
    // Layer 1: the owning class DECLARES its family's paint. This is the
    // whole reason the race is gone — nothing samples the material to find
    // out what the body "was".
    void SetFamilyPaint(const FLinearColor& InFamilyPaint);
    // Layer 2. A demotion is just a rank going back to Trash: it resolves to
    // the family paint with no restore step of any kind.
    void SetRank(EBreakerMonsterRank InRank);
    // Layer 3. Off by default — the dummy has no health axis and opts out
    // rather than being handed a fraction that never moves.
    void SetHealthRampEnabled(bool bEnabled);
    void SetHealthFraction(float Fraction);

    // The body answers a hit. No-op while the death beat owns the materials.
    void NotifyHit(bool bWeakPoint);

    void StartDeathPresentation(bool bWeakPointKill);
    // The revive restore: settles any reaction, returns scale, recomputes the
    // resting colour. Safe to call when nothing ran.
    void ResetDeathPresentation();
    bool IsDeathPresentationActive() const { return DeathPresentationElapsed >= 0.0f; }

    // What the body is painted right now, for tests and for the probe. Pure
    // read of the state — it never touches a material.
    FLinearColor GetResolvedBodyColor() const { return BreakerBodyPaint::Resolve(Paint); }
    const BreakerBodyPaint::FState& GetPaintState() const { return Paint; }

    // Fires once when the crumple lands; the owner hides its body here.
    FSimpleMulticastDelegate OnDeathPresentationFinished;

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    // Resolve once, write to every part. The only place this module writes
    // `Color`, and the only place it is written on the body at all.
    void ApplyBodyPaint();
    void EndHitFlash();
    void UpdateDeathPresentation(float DeltaSeconds);

    TArray<TWeakObjectPtr<UStaticMeshComponent>> Parts;
    TWeakObjectPtr<class UMeshComponent> OverlayBody;
    BreakerBodyPaint::FState Paint;
    FTimerHandle HitFlashTimer;
    float DeathPresentationElapsed = -1.0f;
    bool bDeathBeatWeakPoint = false;
    bool bDeathPresentationRan = false;
    FVector DeathBaseScale = FVector::OneVector;
};
