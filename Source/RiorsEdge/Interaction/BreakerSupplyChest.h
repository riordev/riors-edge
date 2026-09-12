#pragma once

#include "CoreMinimal.h"
#include "Interaction/BreakerNPC.h"
#include "BreakerSupplyChest.generated.h"

class ABreakerCharacter;

// ---------------------------------------------------------------------------
// A SUPPLY CHEST — the small reward standing in the open ground between fights.
//
// Owner-asked, and the shape is deliberately the CACHE'S rather than a new
// kind of thing: ABreakerFernhallCache already proved that an interactable
// reward is an ABreakerNPC subclass with a reachability check and a prompt,
// which is what puts it on the F key with no change in Characters/ or UI/ —
// neither of which this lane owns.
//
// WHAT MAKES IT NOT A CACHE is the gate. A cache is guarded: it will not open
// until its pocket is cleared, and that is the whole point of it. A chest is
// UNGUARDED and pays less for exactly that reason — the rules for how much
// less are in BreakerSupplyChestMath.h, where a bare test can read them.
//
// O280: A CRATE WITH A LID, NOT A CUBE. The body is the composer's
// prop_chest_body, grounded at the capsule's bottom; the lid is prop_chest_lid
// hung from its hinge and swung open on first use. The gold band (O179) wraps
// the seam between them. When either mesh is absent the cube assembly stands
// in, so a clone without the props still has a chest to open.
// ---------------------------------------------------------------------------
UCLASS()
class RIORSEDGE_API ABreakerSupplyChest : public ABreakerNPC
{
    GENERATED_BODY()

public:
    ABreakerSupplyChest();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // The area level its contents are rolled at, and the seed those contents
    // are a pure function of. Configured once, at placement.
    void Configure(int32 AreaLevel, int32 Seed);

    bool TryOpen(ABreakerCharacter* Player);
    bool IsInteractionReachable(const ABreakerCharacter* Player) const;
    bool IsOpened() const { return bOpened; }
    FText GetChestPrompt() const;

    // THE LID'S SWING. The lid mesh's origin is its hinge, so a rotation about
    // the component's local X or Y opens it — which of the two depends on how
    // the composer laid the lid out, and that is not known until the asset is
    // imported. Editable so the owner turns the axis in the details panel
    // rather than waiting on a build. O2 PLACEHOLDER: -70 degrees of pitch,
    // 0.35 s, both felt by nobody yet.
    // The lid swings about the hinge line, which runs along its local X: a
    // positive roll lifts the lip. O2 PLACEHOLDER.
    UPROPERTY(EditAnywhere, Category = "Chest") FRotator LidOpenRotation = FRotator(0.0f, 0.0f, 70.0f);
    UPROPERTY(EditAnywhere, Category = "Chest") float LidOpenSeconds = 0.35f;

    // HOW A CHEST IS FOUND. Measured, not guessed: replaying the owner's own
    // session seed put four of six chests 14 to 19 metres off a lane he walks
    // down the middle of, and a dark 90 cm box at that range is nothing. The
    // spread is RIGHT — being off the lane is the whole reward for leaving it —
    // so what was missing is that the chest never said it was there.
    //
    // A small gold mote above the lid, on a slow breath. Gold because O179
    // spends gold on reward; a breath because the pocket tear proved a still
    // light reads as scenery and a moving one reads as a thing. It goes out
    // when the chest is opened, which is the only tell that it is spent.
    // All O2 PLACEHOLDER.
    // MEASURED FROM THE LID'S TOP, not from the capsule's bottom. The first
    // version took it from the bottom and put the mote three centimetres above
    // the lid, where the capture showed it sunk into the surface as a gold
    // smear. When the crate props load, the lid's top is read from the lid
    // mesh's own bounds and this constant is the CUBE FALLBACK only: the
    // fallback band's top edge sits at relative Z -30.5.
    static constexpr float GlintLidTopCm = -30.5f;
    static constexpr float GlintHeightCm = 34.0f;
    static constexpr float GlintSizeCm = 17.0f;
    static constexpr float GlintHz = 0.55f;
    static constexpr float GlintLow = 1.4f;
    static constexpr float GlintHigh = 4.2f;

    // THE CRATE'S PLACEMENT, all O2 PLACEHOLDER until the props are in.
    // The body stands on the capsule's bottom (InitCapsuleSize 34, 88).
    static constexpr float CrateFloorCm = -88.0f;
    // Where the composer puts the hinge on the body: 0.302 of a 0.534 m body,
    // the kit's own joint. O2 PLACEHOLDER, read from the export log.
    static constexpr float HingeHeightFraction = 0.3018f / 0.5336f;
    // How far the seam band stands proud of the body on each side.
    static constexpr float BandClearanceCm = 5.0f;
    // The band's height, in metres of cube scale.
    static constexpr float BandThickness = 0.05f;
    // The band and seam when only the body's rough size is known (the
    // finding's 1.15 x 0.65 m at a 55 cm seam) — used if the body loads but
    // reports empty bounds, which an import can do before its build.
    static constexpr float BandFallbackLong = 1.15f;
    static constexpr float BandFallbackWide = 0.65f;
    static constexpr float SeamFallbackCm = -33.0f;

private:
    // The lid, hung from BodyMesh at the hinge. Created here because
    // SetupAttachment is constructor-only — the Glint pattern.
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> Lid;
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> Glint;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> GlintMaterial;
    float GlintAge = 0.0f;
    // Seconds since the lid began to swing; only advances once opened.
    float LidOpenAge = 0.0f;
    int32 ItemLevel = 1;
    int32 ContentSeed = 0;
    bool bConfigured = false;
    UPROPERTY(Replicated) bool bOpened = false;
};
