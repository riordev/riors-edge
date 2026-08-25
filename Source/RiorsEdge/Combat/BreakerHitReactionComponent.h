#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BreakerHitReactionComponent.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// THE REACTION LAYER, AS A COMPONENT ANYTHING SHOOTABLE CAN CARRY.
//
// Every flinch, hit flash and death crumple in the game lived on
// ABreakerEnemy, and the gym's target dummy is a bare actor — which is the
// whole explanation for the dead gym: the feedback existed and fired only
// on things the gym doesn't spawn. Ruled: extract, don't promote — a dummy
// that inherited ABreakerEnemy to get a flash would stop being a dummy.
//
// What this owns (moved verbatim from the enemy, behaviour unchanged):
//  * the HIT FLASH — a one-blink material pulse across the registered
//    parts, gold on a weak point, capture-from-rest so a flash landing
//    mid-flash can never stick the body white;
//  * the DEATH PRESENTATION — beat one THE POP (whole-actor scale swell in
//    the flash colour, harder and gold on a weak-point kill), beat two THE
//    CRUMPLE (squash to the ground, colour burning down to ash);
//  * the revive restore (Wakeful) — scale and captured colours returned
//    exactly, including subclass repaints, because capture reads the
//    CURRENT material colour and never the constructor's.
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

    // The meshes the flash and the burn paint. Registered once the owner's
    // parts are final (BeginPlay); order is irrelevant, null parts are
    // skipped, and parts painted later by a subclass are fine because the
    // colour capture happens at flash time.
    void RegisterPart(UStaticMeshComponent* Part);

    // The body answers a hit. No-op while the death beat owns the materials.
    void NotifyHit(bool bWeakPoint);

    void StartDeathPresentation(bool bWeakPointKill);
    // The revive restore: settles any flash, returns scale and colours.
    // Safe to call when nothing ran.
    void ResetDeathPresentation();
    bool IsDeathPresentationActive() const { return DeathPresentationElapsed >= 0.0f; }

    // Fires once when the crumple lands; the owner hides its body here.
    FSimpleMulticastDelegate OnDeathPresentationFinished;

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    void CaptureBodyMaterials();
    void EndHitFlash();
    void UpdateDeathPresentation(float DeltaSeconds);

    TArray<TWeakObjectPtr<UStaticMeshComponent>> Parts;
    TArray<TPair<TWeakObjectPtr<UMaterialInstanceDynamic>, FLinearColor>> BodyMaterialBases;
    FTimerHandle HitFlashTimer;
    bool bHitFlashActive = false;
    float DeathPresentationElapsed = -1.0f;
    bool bDeathBeatWeakPoint = false;
    bool bDeathPresentationRan = false;
    FVector DeathBaseScale = FVector::OneVector;
};
