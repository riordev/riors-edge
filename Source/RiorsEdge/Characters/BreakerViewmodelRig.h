#pragma once

#include "CoreMinimal.h"
#include "Weapons/BreakerWeaponArchetype.h"
#include "BreakerViewmodelRig.generated.h"

// ---------------------------------------------------------------------------
// The first-person BLOCKOUT, as data.
//
// Why this file exists at all. Before it, the whole viewmodel was five lines of
// SetRelativeScale3D in ABreakerCharacter's constructor and a five-case switch
// in ApplyWeaponPresentation. Three problems, all of them visible on screen:
//
//   1. THE PLAYER COULD NOT TELL WHICH GUN THEY WERE HOLDING. Three cubes,
//      re-scaled. Worse, the three newest archetypes (Burst Rifle, Machinegun,
//      Sidearm) had no case at all and silently wore the rifle's proportions,
//      so a 120-round machinegun and a pocket sidearm were the same object.
//   2. THE ARMS WERE OFF SCREEN. Measured, not guessed: at the shipped
//      transforms both arm blocks project to y=1283 and y=1384 on a 1080-tall
//      frame. The player has been holding an invisible gun with no hands.
//   3. THE GUN READ AS ARCHITECTURE. The proxy used the engine's default grey
//      against the gym's grey concrete at a scale that filled a quarter of the
//      frame; in a screenshot it is indistinguishable from a wall.
//
// Everything here is COMPOSED PRIMITIVES plus dynamic material instances — the
// same asset-free technique the gym dressing uses (Game/BreakerGameMode.cpp's
// ApplyShapeColor) — so a clean clone still plays with no content. This is NOT
// art. It is a readable stand-in whose only job is: the player can name the gun
// in their hands, and can see the recoil move it. Art-And-Modelling-Plan.md §5
// is the brief for what replaces it.
//
// It is deliberately world-free, actor-free maths-and-data in the precedent of
// Combat/BreakerRangedBehavior.h and Weapons/BreakerWeaponMath.h, so the layout
// table is unit-testable without spawning anything.
//
// O2: every value in the default table is a PLACEHOLDER. The whole table is
// overridable per archetype on the character instance
// (ABreakerCharacter::ViewmodelLayoutOverrides) with no recompile, exactly like
// UBreakerWeaponComponent::RecoilOverrides.
//
// O24 / the object-chroma law: the reserved saturated teal band belongs to
// rift and suppression OBJECTS. Nothing the player wears or carries may use it.
// The palette below is militia hardware only — gunmetal, olive polymer, one
// hazard amber on the one weapon that is field-fabricated. Enforced by test.
// ---------------------------------------------------------------------------

// Which engine primitive a part is built from, and which rig axis it runs
// along. The engine's BasicShapes cylinders and cones are authored along their
// own local Z; the rig authors along X (forward), so the builder folds in the
// intrinsic rotation and the author never has to think about it.
UENUM(BlueprintType)
enum class EBreakerProxyShape : uint8
{
    // Unused pool slot. A layout with fewer parts than the pool leaves the
    // remainder as None and the builder hides them.
    None,
    Box,
    // Cylinder whose axis lies along rig X: barrels, tubes, scope bodies. A
    // cylinder has no direction, so only the axis matters.
    CylinderX,
    // Cylinder running along rig Y (lateral): the machinegun's drum.
    CylinderY,
    // Cone with its BASE forward — a flare, not a spike. Author a yaw of 180
    // to point it the other way, which is what a back-blast cone wants.
    ConeX
};

// One primitive of the proxy.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerProxyPart
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Viewmodel")
    EBreakerProxyShape Shape = EBreakerProxyShape::None;

    // Centre of the part in RIG SPACE, centimetres. X forward, Y right, Z up.
    // The rig origin sits at the firing hand / trigger group, which is why
    // stocks author negative X and barrels author positive.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Viewmodel")
    FVector LocationCm = FVector::ZeroVector;

    // REAL-WORLD SIZE in centimetres, in rig axes: X length, Y width, Z height.
    // Authoring in centimetres rather than in mesh-scale multiples is the whole
    // reason this is reviewable — "a 32 cm receiver" is a claim you can argue
    // with; "SetRelativeScale3D(0.34, 0.12, 0.11)" is not.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Viewmodel")
    FVector SizeCm = FVector::ZeroVector;

    // Author rotation applied ON TOP of the shape's intrinsic axis rotation:
    // the rake on a magazine, the splay on a bipod leg.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Viewmodel")
    FRotator Rotation = FRotator::ZeroRotator;

    // Linear colour fed to the dynamic material instance. Values are dark on
    // purpose: the gym is bright concrete under a bright sky, and a mid-grey
    // proxy disappears into it (that is exactly the bug this pass fixes).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Viewmodel")
    FLinearColor Color = FLinearColor(0.075f, 0.082f, 0.088f);

    bool IsUsed() const { return Shape != EBreakerProxyShape::None; }
};

// The complete first-person presentation of one archetype.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerViewmodelLayout
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Viewmodel")
    TArray<FBreakerProxyPart> Parts;

    // Where the rig origin sits relative to the camera when hip firing,
    // centimetres. Bigger weapons sit further out and lower; the sidearm sits
    // closer to the centre line, which is how a pistol is actually held.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Viewmodel")
    FVector HipOffsetCm = FVector(26.0f, 13.0f, -16.0f);

    // Height of this weapon's SIGHTING LINE above the rig origin. The ADS rest
    // pose is derived from it rather than authored, so every archetype puts its
    // own sight on the crosshair instead of each one needing a hand-tuned
    // offset that drifts the moment a part moves.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Viewmodel")
    float SightHeightCm = 8.0f;

    // How far forward the rig comes when aiming. Small: ADS is a settle onto
    // the sights, not a lunge.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Viewmodel")
    float AdsForwardCm = 30.0f;

    // Where the two hands land, in rig space. The support hand is what makes a
    // long gun read as held rather than floating, and it MOVES per archetype —
    // out on the handguard for a rifle, back on the pump for a shotgun, absent
    // in spirit for the sidearm (both hands stack at the grip).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Viewmodel")
    FVector SupportHandCm = FVector(34.0f, 0.0f, -4.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Viewmodel")
    FVector FiringHandCm = FVector(-1.0f, 0.0f, -7.0f);

    // Muzzle position in rig space: where the flash light hangs. Attached to
    // the rig rather than to the camera so the flash rides the recoil spring
    // with the gun instead of staying nailed to the screen.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Viewmodel")
    FVector MuzzleCm = FVector(60.0f, 0.0f, 2.5f);

    // Longest dimension of the assembled weapon, used by the readability test
    // to pin the ORDERING of the silhouettes (sidearm < SMG < ... < sniper)
    // without pinning the values, which are frozen under O2.
    float OverallLengthCm() const;

    // --- THE NAMED GUN (asset-intake wiring) ------------------------------
    // When this resolves to a static mesh it replaces the proxy PARTS whole:
    // the intake gun scales so its longest bound equals OverallLengthCm() —
    // the same figure the silhouette-ordering law reads, so a named sidearm
    // stays shorter than a named sniper by construction — and its bounds
    // centre lands halfway to the muzzle. Arms, muzzle flash and the recoil
    // rig are untouched: the named gun rides the same driven transform. Unset
    // (Shotgun and Rocket — no pack ships a candidate) the primitives stand,
    // and a clean clone without Content still plays. All O2.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Viewmodel")
    FSoftObjectPath NamedMeshPath;
    // Source-axis correction onto rig X-forward, per pack convention.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Viewmodel")
    FRotator NamedMeshRotation = FRotator::ZeroRotator;
};

namespace BreakerViewmodel
{
    // ---- Palette -------------------------------------------------------
    // Militia hardware, per Art-And-Modelling-Plan.md §3.1 and Pillar 3.
    // Desaturated steel and olive polymer; hazard amber appears on exactly one
    // weapon. No teal anywhere — O24 and the object-chroma law.
    // MEASURED against a screenshot, not picked from a swatch. The gym runs
    // auto-exposure under a bright sky and its concrete is authored at linear
    // 0.33; a first pass at linear 0.08 came back mid-grey and vanished into
    // the floor. These are roughly a quarter of that pass and the SPREAD
    // between them (0.010 to 0.075, about 7x) is what carries the read, because
    // value contrast is the only budget a flat blockout has.
    inline const FLinearColor Gunmetal      (0.022f, 0.024f, 0.026f);
    inline const FLinearColor GunmetalDark  (0.010f, 0.011f, 0.012f);
    inline const FLinearColor Polymer       (0.018f, 0.022f, 0.017f);
    inline const FLinearColor PolymerCheap  (0.040f, 0.043f, 0.037f);
    inline const FLinearColor SteelBright   (0.075f, 0.080f, 0.082f);
    inline const FLinearColor HazardAmber   (0.300f, 0.170f, 0.030f);
    inline const FLinearColor TapeOffWhite  (0.160f, 0.150f, 0.125f);
    inline const FLinearColor GloveOlive    (0.014f, 0.017f, 0.013f);
    inline const FLinearColor SleeveSlate   (0.028f, 0.032f, 0.034f);

    // The largest part count any archetype uses. The character allocates this
    // many components once in its constructor and recycles them, so switching
    // weapons never spawns anything — the same discipline
    // ABreakerTracerRenderer follows for rounds in flight.
    inline constexpr int32 MaxProxyParts = 12;

    // The default table. O2 PLACEHOLDER throughout.
    RIORSEDGE_API FBreakerViewmodelLayout ArchetypeLayout(EBreakerWeaponArchetype Archetype);

    // The named gun's fit, pure so it is provable without a component: scale
    // the mesh's longest bound to the layout's overall length (silhouette
    // ordering survives by construction), cancel the bounds origin at that
    // scale, and land the bounds centre at CentreAtCm in rig space. Degenerate
    // bounds refuse the fit at identity, the same rule as the enemy body.
    //
    // The cancel happens THROUGH the source-axis rotation the component wears:
    // a relative transform scales, then rotates, then translates, so a pivot
    // sitting off the bounds centre rotates with the mesh before the location
    // lands. The first fit cancelled it unrotated, which under the pack's
    // 180° yaw would have put a gun 2·Scale·(Ox, Oy, 0) from where the fit
    // said — invisible on a mesh pivoted at its bounds centre, a full
    // receiver-length off on one pivoted at the grip.
    inline void FitNamedWeapon(const FVector& BoundsOrigin, const FVector& BoundsExtent,
        const float TargetLengthCm, const FVector& CentreAtCm, const FQuat& Rotation,
        float& OutScale, FVector& OutLocationCm)
    {
        OutScale = 1.0f;
        OutLocationCm = FVector::ZeroVector;
        const float LongestHalf = static_cast<float>(BoundsExtent.GetMax());
        if (LongestHalf <= UE_KINDA_SMALL_NUMBER || TargetLengthCm <= UE_KINDA_SMALL_NUMBER)
        {
            return;
        }
        OutScale = (TargetLengthCm * 0.5f) / LongestHalf;
        OutLocationCm = CentreAtCm - Rotation.RotateVector(BoundsOrigin * OutScale);
    }

    // Which way a gun mesh points, read from its geometry rather than from a
    // photograph. Take the longest bounds axis; compare the front and back
    // QUARTER slabs of vertices along it by their largest perpendicular
    // extent. A gun's barrel end is the thin one — stock, grip, magazine and
    // receiver all sit fatter than a barrel in every pack this project has
    // vendored — so the muzzle is the thinner slab. Returns a unit axis in
    // MESH space (±X, ±Y or ±Z); the caller rotates it by the layout's
    // NamedMeshRotation and expects rig +X. Degenerate or ambiguous input
    // (no vertices, a cube, slabs within 5 % of each other) returns Zero so
    // the caller fails loudly instead of trusting a coin flip.
    inline FVector MuzzleAxisFromVertices(TArrayView<const FVector3f> Positions)
    {
        if (Positions.Num() < 4) return FVector::ZeroVector;
        FVector3f Min(FLT_MAX), Max(-FLT_MAX);
        for (const FVector3f& P : Positions)
        {
            Min = FVector3f::Min(Min, P);
            Max = FVector3f::Max(Max, P);
        }
        const FVector3f Size = Max - Min;
        int32 Axis = 0;
        if (Size.Y > Size[Axis]) Axis = 1;
        if (Size.Z > Size[Axis]) Axis = 2;
        const float Length = Size[Axis];
        if (Length <= UE_KINDA_SMALL_NUMBER) return FVector::ZeroVector;
        const int32 SideA = (Axis + 1) % 3;
        const int32 SideB = (Axis + 2) % 3;
        const float FrontFrom = Max[Axis] - 0.25f * Length;
        const float BackTo = Min[Axis] + 0.25f * Length;
        FVector2f FrontMin(FLT_MAX), FrontMax(-FLT_MAX), BackMin(FLT_MAX), BackMax(-FLT_MAX);
        for (const FVector3f& P : Positions)
        {
            const FVector2f Side(P[SideA], P[SideB]);
            if (P[Axis] >= FrontFrom)
            {
                FrontMin = FVector2f::Min(FrontMin, Side);
                FrontMax = FVector2f::Max(FrontMax, Side);
            }
            if (P[Axis] <= BackTo)
            {
                BackMin = FVector2f::Min(BackMin, Side);
                BackMax = FVector2f::Max(BackMax, Side);
            }
        }
        const float FrontGirth = (FrontMax - FrontMin).GetMax();
        const float BackGirth = (BackMax - BackMin).GetMax();
        const float Fatter = FMath::Max(FrontGirth, BackGirth);
        if (Fatter <= UE_KINDA_SMALL_NUMBER) return FVector::ZeroVector;
        if (FMath::Abs(FrontGirth - BackGirth) / Fatter < 0.05f) return FVector::ZeroVector;
        FVector Out = FVector::ZeroVector;
        Out[Axis] = (FrontGirth < BackGirth) ? 1.0 : -1.0;
        return Out;
    }

    // Component scale and total rotation for a part, folding in the intrinsic
    // axis rotation of cylinders and cones. Pure; the character only applies it.
    RIORSEDGE_API void ResolvePartTransform(const FBreakerProxyPart& Part, FVector& OutScale, FRotator& OutRotation);

    // A limb drawn as one stretched box from an anchor to a hand. Returns the
    // centre, the rotation, and the length so the caller can size the box.
    RIORSEDGE_API void ResolveLimb(const FVector& AnchorCm, const FVector& HandCm,
        FVector& OutCentreCm, FRotator& OutRotation, float& OutLengthCm);
}
