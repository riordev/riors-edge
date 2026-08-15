#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerEnemy.h"
#include "BreakerAlteredEnemy.generated.h"

// SEVERED DRUDGE — a LATE-severance Altered, built as a chassis, not an alien
// (owner ruling: an externally-blocked-out creature was reworked as Altered
// rather than shipped as an alien; there are no aliens in this game —
// Story-Source §1.5 names exactly two families, Vestiges and the Altered, and
// both are severed HUMANS. See Story-Source.md's "the longer a refugee is
// severed, the further they degrade, until nothing is left but the shape.").
//
// WHY LATE AND NOT MID OR EARLY. §1.5's ladder: Early still wears insignia,
// takes cover, flinches — that is Skirmisher. Mid still carries and uses
// equipment but has lost the tactics — that is Warden, sweep and slam. Late
// is the sentence quoted above, verbatim: nothing left but the shape. This
// archetype carries no equipment, no weapon, no insignia, and — per the
// brief — inherits nothing but the base three-gear melee chase, which is
// exactly the Late-stage mechanical claim: UsesCoverDiscipline() and
// FlinchesWhenHit() both resolve false for (Altered, Late), and the chassis
// never contradicts that by taking cover or flinching, because it was never
// taught to.
//
// THE SOURCE BLOCKOUT, and what was kept from it. An external asset defined a
// sprawling five-limbed mass in centimetres — off-fiction (no exoskeletons,
// no extra limbs, no mandibles: Story-Source's Altered are humans months into
// severance and the skeleton must stay legible) and off-collision (it did not
// fit inside anything the game's hit-registration understands). What survives
// is the IDEA, not the geometry: a low, heavy, asymmetric, weight-dragging
// silhouette — a dominant forward mass, one oversized arm and one withered
// one, a dragging rear, and a raised dorsal ridge as the exposed spot. Every
// primitive below is hand-authored at Rior's Edge offsets, not imported.
//
// WHY THE CAPSULE IS OVERRIDDEN. The base chassis' 45x90 capsule (90 cm
// diameter) cannot hold an asymmetric silhouette with one OVERSIZED limb
// without either clipping the limb through the collision boundary or
// shrinking it into the same silhouette as everything else — and "the models
// just slowly walk at me" is exactly the sameness this archetype exists to
// break. See ABreakerAlteredEnemy() for the new dimensions and the arithmetic
// that keeps the weak point's world height matching its visual tell, which is
// the constraint the external asset failed by design (this document's own
// coordinator brief: "a visual tell that does not coincide with the collision
// sphere is a recorded defect elsewhere in this codebase").
//
// NO NEW ATTACK. This is a chassis lane, not a behaviour lane: the base
// class's three-gear melee chase (sprint / weave / lunge) runs unmodified
// except for the archetype's own speed and weave tuning below, exactly as
// Story-Source's "nothing left but the shape" implies — it does not fight
// like a soldier, it just closes.
//
// EVERY NUMBER IS AN O2 PLACEHOLDER and none of it is playtested.
UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerAlteredEnemy : public ABreakerEnemy
{
    GENERATED_BODY()

public:
    ABreakerAlteredEnemy();

    // --- Test-support accessors ---------------------------------------------
    // BodyCollision and WeakPoint are protected on the base class (by design —
    // see BreakerEnemy.h), so a world-free automation test cannot reach them
    // to check the arithmetic this archetype's whole capsule-override
    // justification rests on. These expose exactly the numbers the class
    // comment's arithmetic depends on and nothing else; they do not widen
    // access to the components themselves.
    UFUNCTION(BlueprintPure, Category="Enemy|Altered") float GetCapsuleRadiusCm() const;
    UFUNCTION(BlueprintPure, Category="Enemy|Altered") float GetCapsuleHalfHeightCm() const;
    // World height of the WEAK POINT SPHERE's centre above the ground, i.e.
    // CapsuleHalfHeight + WeakPoint's relative Z — the number the class
    // comment derives by hand and the test pins.
    UFUNCTION(BlueprintPure, Category="Enemy|Altered") float GetWeakPointWorldHeightCm() const;
    UFUNCTION(BlueprintPure, Category="Enemy|Altered") float GetWeakPointRadiusCm() const;

    // Conservative (pre-rotation, axis-aligned) check that every cosmetic
    // humanoid primitive's extent stays inside the capsule this archetype
    // authors — the exact constraint the source blockout failed. Basic-shape
    // Cube and Sphere meshes are both 100 cm across before scale, so a
    // component's half-extent on an axis is 50 * (that axis' scale).
    UFUNCTION(BlueprintPure, Category="Enemy|Altered") bool AreCosmeticExtentsWithinCapsule() const;
};
