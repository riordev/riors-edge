#include "Combat/BreakerAlteredEnemy.h"

#include "Combat/BreakerBodyPaint.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace BreakerAlteredDetail
{
    // Distinctively prefixed for the unity build, same reason as the Warden's
    // BreakerWardenDetail namespace.
    //
    // PALETTE (Decisions.md O24, Art-And-Modelling-Plan.md §Pillar 3 "Human /
    // militia" reservation): "desaturated olive-slate #4A5049, warm off-white
    // insignia #D8CFBA, hazard amber #D89A2E." Teal (#3FD8C8) is reserved for
    // rift phenomena and never appears here. Values below are the hex bytes
    // divided by 255, matching how BreakerEnemy.cpp's own ApplyEnemyBodyColor
    // expresses its Vestige tint — FLinearColor(0.35f, 0.32f, 0.42f), a muted
    // grey-violet — as a direct 0-1 vector rather than a gamma-corrected linear
    // one. The Altered read has to be a different HUE from that Vestige
    // grey-violet, not just a different brightness, or the two families would
    // silhouette identically at a glance, which defeats the whole point of a
    // second family.
    // #4A5049, and the SAME symbol the paint resolver defaults this family
    // to: a family's declared paint and its painted paint are one value or
    // they drift (O128).
    const FLinearColor OliveSlate = BreakerBodyPaint::AlteredFamilyPaint;
    const FLinearColor HazardAmber(0xD8 / 255.0f, 0x9A / 255.0f, 0x2E / 255.0f);     // #D89A2E

    static UMaterialInstanceDynamic* BreakerMakeAlteredMaterial(UStaticMeshComponent* Mesh, const FLinearColor& Color)
    {
        if (!Mesh) return nullptr;
        UMaterialInterface* Base = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (!Base) return nullptr;
        UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Base, Mesh);
        if (!Dynamic) return nullptr;
        Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
        Mesh->SetMaterial(0, Dynamic);
        return Dynamic;
    }
}

ABreakerAlteredEnemy::ABreakerAlteredEnemy()
{
    // Late severance wears Mike at a walk (owner's mech-cast ruling, O2
    // mapping) — "nothing is left but the shape", and now the shape is a hull.
    BodyMeshAsset = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/enemies/mechs/Mike/Mike.Mike"));
    BodyIdleAnimation = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/enemies/mechs/Mike/MikeRobotArmature_Walk.MikeRobotArmature_Walk"));
    // --- FAMILY (Assets/story-source.md §1.5) ----------------------------------------
    // LATE severance: "nothing is left but the shape." No equipment, no
    // insignia, no tactics — UsesCoverDiscipline() and FlinchesWhenHit() both
    // read false for (Altered, Late) through UBreakerEnemyFamilyLibrary, which
    // is the mechanical form of the same sentence, and it is why this
    // archetype adds no behaviour at all: there is nothing left to add.
    Family = EBreakerEnemyFamily::Altered;
    // Layer 1, declared. Everything below paints from it.
    FamilyPaint = BreakerAlteredDetail::OliveSlate;
    SeveranceStage = EBreakerSeveranceStage::Late;

    // --- CHASSIS (all O2 PLACEHOLDER, derived against the shipped roster) --
    // Skitter (the melee baseline) ships at the archetype-multiplier default
    // of 1.0/1.0. The Warden's anchor identity earns 3.2x health, but 90 of
    // its cm is FRONTAL ARMOUR (roughly 47% mitigation from the front) — this
    // archetype carries none, so matching the Warden's raw multiplier would
    // make an unarmoured enemy tankier than an armoured one from every angle
    // except the one the Warden is actually weak from. 2.0x sits below that
    // for exactly that reason: a heavy, slow, unarmoured mass that dies faster
    // than the Warden once you are hitting it, but is bulkier than the
    // baseline because it IS the bulkiest silhouette in the roster.
    // Damage: the Warden's sweep is 1.86x (its 26 against the 14 baseline).
    // This archetype has no telegraphed special attack, only the base contact
    // hit, so 1.3x is authored below that: it should sting more than a
    // Skitter on contact (it is visibly bigger) without approaching a
    // telegraphed heavy attack's number.
    ArchetypeHealthMultiplier = 2.0f;   // O2 PLACEHOLDER
    ArchetypeDamageMultiplier = 1.3f;   // O2 PLACEHOLDER

    // Slower than the 330 baseline and far under the Warden's 260 anchor pace
    // is wrong — the Warden is deliberately an anchor. This archetype is
    // "weight-dragging" rather than "holds ground", so it sits close to the
    // Warden's pace without copying it outright.
    MoveSpeed = 250.0f;          // O2 PLACEHOLDER
    // The oversized forward arm reads as reach, not as a lunge weapon.
    AttackRange = 300.0f;        // O2 PLACEHOLDER
    // No juke: something this asymmetric and heavy does not weave, the same
    // reasoning the Warden uses for the same field.
    WeaveStrength = 0.0f;
    // No committed leap either. A dragging, weight-heavy silhouette that
    // suddenly leaps contradicts its own silhouette — the Warden zeroes this
    // for an analogous reason (an anchor that leaps is a different archetype)
    // and SKITTER already owns the leap identity.
    LungeRange = 0.0f;
    LungeSpeedMultiplier = 1.0f;

    // --- THE CHASSIS COLLISION ----------------------------------------------
    // OVERRIDDEN from the base 45x90 (90 cm diameter, 180 cm tall). An
    // asymmetric silhouette with one OVERSIZED limb cannot fit inside the base
    // capsule without either clipping the limb outside the collision (the
    // external blockout's exact failure — cosmetic mass with no hit
    // registration, and it clips through walls) or shrinking the limb down to
    // the same silhouette as every other enemy, which defeats the brief.
    //
    // NEW DIMENSIONS: radius 60 cm (120 cm diameter), half-height 75 cm
    // (150 cm tall — shorter than the 180 cm baseline, which is deliberate:
    // "low, heavy" is a silhouette that is WIDER and SHORTER, not just bigger
    // in every direction). Origin is still the capsule's own centre, 75 cm
    // above the ground (down from the base's 90 cm) because the half-height
    // changed.
    static constexpr float CapsuleRadiusCm = 60.0f;
    static constexpr float CapsuleHalfHeightCm = 75.0f;
    BodyCollision->SetCapsuleSize(CapsuleRadiusCm, CapsuleHalfHeightCm, true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    // Every extent below is checked against the new capsule. The capsule's
    // cross-section is a CIRCLE of radius 60, not a square, so the real test
    // for each box primitive is the distance from the capsule's centre axis
    // to the FAR CORNER of the box — sqrt((|Lx|+ExtX)^2 + (|Ly|+ExtY)^2) — not
    // the two axes checked independently (a box can pass an independent-axis
    // check and still have a corner outside a circular capsule; this is
    // exactly the mistake AreCosmeticExtentsWithinCapsule() below does NOT
    // make). Z stays in [-75, 75] relative to BodyCollision's origin (world
    // height 0-150). The base engine primitives are 100 cm across before
    // scale, so a scale of S gives a half-extent of 50*S on that axis.

    // BodyVisual — the dominant forward mass, hunched low over its own feet.
    // Half-extents (33, 25, 27.5) at offset (16, 0, 6): far corner (49, 25),
    // distance sqrt(49^2+25^2) = 55.0 cm, inside the 60 cm radius.
    // Z: -21.5..33.5 — inside +-75.
    BodyVisual->SetRelativeLocation(FVector(16.0f, 0.0f, 6.0f));
    BodyVisual->SetRelativeScale3D(FVector(0.66f, 0.50f, 0.55f));
    BreakerAlteredDetail::BreakerMakeAlteredMaterial(BodyVisual, FamilyPaint);

    // HeadVisual — kept small and low rather than removed: Assets/story-source.md is
    // explicit the skeleton must stay legible, and a body with no head reads
    // as a monster silhouette rather than a person who has degraded.
    // Half-extent 14 at offset (14, 0, 46): X: 0..28, Z: 32..60 — inside.
    HeadVisual->SetRelativeLocation(FVector(14.0f, 0.0f, 46.0f));
    HeadVisual->SetRelativeScale3D(FVector(0.28f));
    BreakerAlteredDetail::BreakerMakeAlteredMaterial(HeadVisual, FamilyPaint);

    // RightArmVisual — THE oversized limb the source blockout was built
    // around. The capsule's cross-section is a CIRCLE (radius 60), not a
    // square, so the check that matters is the diagonal to the box's far
    // corner, not the two axes independently. Half-extents (14, 15, 42.5) at
    // offset (14, 33, -6): far corner at (28, 48), distance
    // sqrt(28^2 + 48^2) = 55.6 cm from the capsule's centre axis, 4.4 cm of
    // margin inside the 60 cm radius — the tightest primitive here, because
    // it is the one the brief singles out as the reason the capsule had to
    // grow at all. Z: -48.5..36.5 — inside the +-75 half-height.
    RightArmVisual->SetRelativeLocation(FVector(14.0f, 33.0f, -6.0f));
    RightArmVisual->SetRelativeScale3D(FVector(0.28f, 0.30f, 0.85f));
    BreakerAlteredDetail::BreakerMakeAlteredMaterial(RightArmVisual, FamilyPaint);

    // LeftArmVisual — the withered counterpart. Small and tucked in close, the
    // opposite read from the dominant right arm.
    // Half-extents (5, 5, 19) at offset (6, -18, 22): far corner (11, 23),
    // distance sqrt(11^2+23^2) = 25.5 cm, well inside. Z: 3..41 — inside.
    LeftArmVisual->SetRelativeLocation(FVector(6.0f, -18.0f, 22.0f));
    LeftArmVisual->SetRelativeScale3D(FVector(0.10f, 0.10f, 0.38f));
    BreakerAlteredDetail::BreakerMakeAlteredMaterial(LeftArmVisual, FamilyPaint);

    // RightLegVisual — planted, stouter, bearing the dominant forward mass.
    // Half-extents (13, 14, 33) at offset (4, 16, -40): far corner (17, 30),
    // distance sqrt(17^2+30^2) = 34.5 cm, well inside. Z: -73..-7 — 2 cm of
    // margin to the floor of the capsule (Z = -75), which is deliberately
    // tight: this leg reaches almost to the ground snap the base Tick already
    // performs.
    RightLegVisual->SetRelativeLocation(FVector(4.0f, 16.0f, -40.0f));
    RightLegVisual->SetRelativeScale3D(FVector(0.26f, 0.28f, 0.66f));
    BreakerAlteredDetail::BreakerMakeAlteredMaterial(RightLegVisual, FamilyPaint);

    // LeftLegVisual — the dragging rear leg: splayed back and out rather than
    // planted under the body, which is the "dragging rear" half of the source
    // silhouette read without the extra limb the blockout used to sell it.
    // Half-extents (11, 12, 29) at offset (-24, -28, -36), yawed 18 degrees
    // outward (the check below is pre-rotation, axis-aligned, and therefore
    // conservative for a yaw about the component's own pivot): far corner
    // (35, 40), distance sqrt(35^2+40^2) = 53.2 cm, 6.8 cm of margin inside
    // the 60 cm radius — comfortable enough to absorb the yaw.
    // Z: -65..-7 — 10 cm of margin to the floor.
    LeftLegVisual->SetRelativeLocation(FVector(-24.0f, -28.0f, -36.0f));
    LeftLegVisual->SetRelativeRotation(FRotator(0.0f, 18.0f, 0.0f));
    LeftLegVisual->SetRelativeScale3D(FVector(0.22f, 0.24f, 0.58f));
    BreakerAlteredDetail::BreakerMakeAlteredMaterial(LeftLegVisual, FamilyPaint);

    // BodyHitBox — resized to match the new frame. Unlike the six cosmetic
    // meshes above, this does NOT have to fit the capsule's circular
    // cross-section: it is real collision in its own right (like the capsule
    // itself), and the base class's own BodyHitBox already extends past ITS
    // capsule's radius on the diagonal (extent (42, 42) against a 45 cm
    // radius — far-corner distance 59.4 cm — confirmed against
    // BreakerEnemy.cpp:116-119). This one is sized to the new frame's Z range
    // (top 46, bottom -54, both inside the +-75 half-height) without chasing
    // the circular constraint that only applies to the NoCollision cosmetic
    // set.
    BodyHitBox->SetBoxExtent(FVector(48.0f, 45.0f, 50.0f));
    BodyHitBox->SetRelativeLocation(FVector(10.0f, 0.0f, -4.0f));

    // --- THE WEAK POINT: the raised dorsal ridge ---------------------------
    // The source blockout's "raised dorsal knuckle" becomes the archetype's
    // exposed spot, in the same USphereComponent the base class already wires
    // for damage — repositioned, not recreated. Its radius (20) is left at
    // the base value; only the placement changes.
    //
    // Relative location (-24, 0, 48): X is negative (rearward — this is the
    // BACK of the frame, "dorsal"), Y is 0 (kept on the centreline exactly
    // like the base class's own weak point, so it reads as a single ridge
    // along the spine rather than something off to one side), Z is raised
    // toward the top of the shorter frame.
    //
    // SEATED ON THE BODY, not floating near it. The first authored placement
    // (-30, 0, 54) left the sphere's surface ~4.3 cm clear of BodyVisual's
    // nearest corner — a weak point visibly hanging in the air behind the
    // ridge it claims to be part of, which reads as a bug the moment the
    // player circles it. At (-24, 0, 48) the closest point of BodyVisual's
    // box (X -17..49, Z -21.5..33.5 — see its comment above) is (-17, 0,
    // 33.5), which is sqrt(7^2 + 14.5^2) = 16.1 cm from the sphere's centre:
    // 3.9 cm INSIDE the 20 cm radius, so the sphere visibly overlaps the
    // torso's rear-top edge and reads as a ridge growing out of the back.
    // DoesWeakPointTouchCosmetics() below asserts exactly this and the
    // geometry test pins it.
    //
    // WORLD HEIGHT ARITHMETIC (this is the number that matters — a visual
    // tell that does not coincide with the collision sphere is a recorded
    // defect elsewhere in this codebase, and it does not get a second one
    // here): capsule origin sits CapsuleHalfHeightCm (75 cm) above the ground
    // after the ground-snap in the base Tick, and WeakPoint's relative Z here
    // is 48, so the sphere's CENTRE is 75 + 48 = 123 cm above the ground, on
    // the centreline. WeakPointVisual is a child of WeakPoint and was never
    // touched, so the visible tell sits at exactly that point — there is only
    // one number to keep in sync, and it lives here.
    //
    // Sphere bounds against the capsule: X: -44..-4, Z: 28..68 — the top of
    // the sphere (68) is 7 cm inside the capsule ceiling (75), which is
    // MORE conservative than the base class's own weak point (78 relative +
    // 20 radius = 98, against a 90 half-height — it already pokes 8 cm past
    // its own capsule top and ships that way).
    WeakPoint->SetRelativeLocation(FVector(-24.0f, 0.0f, 48.0f));

    // The tell is recoloured hazard-amber so it reads as the exposed spot
    // against the olive-slate body, the same "distinct hot colour on the
    // thing you're meant to shoot" idiom the Warden uses for its shield during
    // wind-up. This is a recolour of the EXISTING WeakPointVisual the base
    // class creates and attaches to WeakPoint — no second mesh is added.
    if (SphereMesh.Succeeded() && WeakPointVisual && !WeakPointVisual->GetStaticMesh())
    {
        WeakPointVisual->SetStaticMesh(SphereMesh.Object);
    }
    BreakerAlteredDetail::BreakerMakeAlteredMaterial(WeakPointVisual, BreakerAlteredDetail::HazardAmber);
}

float ABreakerAlteredEnemy::GetCapsuleRadiusCm() const
{
    return BodyCollision ? BodyCollision->GetUnscaledCapsuleRadius() : 0.0f;
}

float ABreakerAlteredEnemy::GetCapsuleHalfHeightCm() const
{
    return BodyCollision ? BodyCollision->GetUnscaledCapsuleHalfHeight() : 0.0f;
}

float ABreakerAlteredEnemy::GetWeakPointWorldHeightCm() const
{
    if (!BodyCollision || !WeakPoint) return 0.0f;
    // Unscaled: the archetype default has no actor-scale applied (that is an
    // elite-only runtime multiplier, ConfigureElite(), not a chassis fact),
    // matching how the class comment's arithmetic is stated.
    return BodyCollision->GetUnscaledCapsuleHalfHeight() + WeakPoint->GetRelativeLocation().Z;
}

float ABreakerAlteredEnemy::GetWeakPointRadiusCm() const
{
    return WeakPoint ? WeakPoint->GetUnscaledSphereRadius() : 0.0f;
}

bool ABreakerAlteredEnemy::AreCosmeticExtentsWithinCapsule() const
{
    if (!BodyCollision) return false;
    const float Radius = BodyCollision->GetUnscaledCapsuleRadius();
    const float HalfHeight = BodyCollision->GetUnscaledCapsuleHalfHeight();

    // The six humanoid primitives BodyHitBox and WeakPoint are collision
    // themselves, not cosmetic, and are checked separately by the tests that
    // read GetWeakPoint*() above. This is deliberately the same list
    // SetBodyVisible toggles.
    const UStaticMeshComponent* Parts[] = {
        BodyVisual, HeadVisual, LeftArmVisual, RightArmVisual, LeftLegVisual, RightLegVisual
    };
    for (const UStaticMeshComponent* Part : Parts)
    {
        if (!Part) continue;
        const FVector Location = Part->GetRelativeLocation();
        const FVector Scale = Part->GetRelativeScale3D();
        // Basic-shape Cube and Sphere are both 100 cm across before scale, so
        // the half-extent on an axis is 50 * that axis' scale. The capsule's
        // cross-section is a CIRCLE, so the horizontal check is the distance
        // from the centre axis to the box's FAR CORNER, not the two axes
        // checked independently — an independent-axis pass can still leave a
        // corner outside a circular capsule, which is exactly the geometry
        // mistake this function exists to not make (see the constructor's
        // per-primitive comments for the by-hand version of this arithmetic).
        // Pre-rotation and axis-aligned, so a yawed component (LeftLegVisual)
        // is checked conservatively rather than exactly.
        const float FarCornerX = FMath::Abs(Location.X) + Scale.X * 50.0f;
        const float FarCornerY = FMath::Abs(Location.Y) + Scale.Y * 50.0f;
        const float HorizontalExtent = FMath::Sqrt(FarCornerX * FarCornerX + FarCornerY * FarCornerY);
        const float Top = Location.Z + Scale.Z * 50.0f;
        const float Bottom = Location.Z - Scale.Z * 50.0f;
        if (HorizontalExtent > Radius || Top > HalfHeight || Bottom < -HalfHeight) return false;
    }
    return true;
}

bool ABreakerAlteredEnemy::DoesWeakPointTouchCosmetics() const
{
    if (!WeakPoint) return false;
    const FVector Center = WeakPoint->GetRelativeLocation();
    const float Radius = WeakPoint->GetUnscaledSphereRadius();

    // Same cosmetic list AreCosmeticExtentsWithinCapsule walks, same
    // pre-rotation axis-aligned reading of each primitive as a box of
    // half-extent 50 * scale (basic-shape meshes are 100 cm across before
    // scale). The sphere "touches" a primitive when the distance from its
    // centre to the closest point of that box is at or under the radius —
    // i.e. the visible sphere reaches the visible mass instead of floating
    // beside it.
    const UStaticMeshComponent* Parts[] = {
        BodyVisual, HeadVisual, LeftArmVisual, RightArmVisual, LeftLegVisual, RightLegVisual
    };
    for (const UStaticMeshComponent* Part : Parts)
    {
        if (!Part) continue;
        const FVector Location = Part->GetRelativeLocation();
        const FVector HalfExtent = Part->GetRelativeScale3D() * 50.0f;
        const FVector Closest(
            FMath::Clamp(Center.X, Location.X - HalfExtent.X, Location.X + HalfExtent.X),
            FMath::Clamp(Center.Y, Location.Y - HalfExtent.Y, Location.Y + HalfExtent.Y),
            FMath::Clamp(Center.Z, Location.Z - HalfExtent.Z, Location.Z + HalfExtent.Z));
        if (FVector::DistSquared(Center, Closest) <= FMath::Square(Radius)) return true;
    }
    return false;
}
