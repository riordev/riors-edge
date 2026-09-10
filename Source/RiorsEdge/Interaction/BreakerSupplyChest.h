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

private:
    int32 ItemLevel = 1;
    int32 ContentSeed = 0;
    bool bConfigured = false;
    UPROPERTY(Replicated) bool bOpened = false;
};
