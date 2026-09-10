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

    // Whether this chest will pay currency rather than an item. Answerable
    // before it is opened because the contents are a pure function of the seed
    // — which is what lets a test walk the distribution without a world.
    bool PaysCurrency() const;

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
    // MEASURED FROM THE LID, not from the capsule's bottom. The first version
    // took it from the bottom and put the mote three centimetres above the lid,
    // where the capture showed it sunk into the surface as a gold smear. The
    // Trim band's top edge sits at relative Z -30.5, so this is the float above
    // that.
    static constexpr float GlintLidTopCm = -30.5f;
    static constexpr float GlintHeightCm = 34.0f;
    static constexpr float GlintSizeCm = 17.0f;
    static constexpr float GlintHz = 0.55f;
    static constexpr float GlintLow = 1.4f;
    static constexpr float GlintHigh = 4.2f;

private:
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> Glint;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> GlintMaterial;
    float GlintAge = 0.0f;
    int32 ItemLevel = 1;
    int32 ContentSeed = 0;
    bool bConfigured = false;
    UPROPERTY(Replicated) bool bOpened = false;
};
