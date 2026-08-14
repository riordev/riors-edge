#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Items/BreakerItemTypes.h"
#include "BreakerForgeLibrary.generated.h"

// ---------------------------------------------------------------------------
// THE FORGE — the smallest item-agency loop that makes T0 and T-1 reachable.
// ---------------------------------------------------------------------------
// The gap this closes, stated exactly. `UBreakerAffixLibrary::TierCapForRarity`
// lets Exceptional and above reach T-1, and `BestTierForItemLevel` stops at T1
// at character level 50 with the comment "T0/T-1 never come from item level —
// they are crafting/boss territory". Item-Foundation's tier scale says the same
// thing. There is no crafting and there is no boss, so the two best tiers in
// the game — the ones the whole tier curve SPIKES toward, T0 at 1.4x T1 and
// T-1 at 1.8x — were unreachable by any means. Two tenths of the item system
// existed only as a comment.
//
// WHAT THIS DELIBERATELY IS NOT. Not an economy. There is no vendor, no
// material drop table, no orb inventory, no bench progression and no
// item-derived materials (O12 rules the currencies SCALAR and tiered, 3-4 of
// them, and that is what this implements). The slice does not want a crafting
// game; it wants the top of its own tier curve to be reachable and it wants a
// reason not to vendor every drop.
//
// THE LOOP: salvage a backpack item -> scalar currency -> temper one affix one
// tier better, reforge an item's values, or attune which affixes it carries.
// Three verbs, all deterministic, all pure functions over an item and a wallet.
//
// FORGE-GATED, like the respec. UBreakerProgressionComponent::RespecAtForge
// takes a bIsAtForge flag and refuses away from one; every operation here takes
// the same flag for the same reason, so "the Forge is a place you go" is one
// rule rather than two.

UENUM(BlueprintType)
enum class EBreakerForgeCurrency : uint8
{
    // O12: scalar and tiered. Three, not four, because a fourth tier with
    // nothing to spend it on is the first step toward the economy this is
    // explicitly not building.
    Slag,   // Common. Salvaged from anything; buys value rerolls and low tiers.
    Flux,   // Uncommon. From Exceptional and above; buys the T3..T1 tempers.
    Sigil,  // Rare. From Aberrant and above; the ONLY thing that buys T0/T-1.
    Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerForgeCost
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) EBreakerForgeCurrency Currency = EBreakerForgeCurrency::Slag;
    UPROPERTY(BlueprintReadOnly) int32 Amount = 0;

    bool IsFree() const { return Amount <= 0; }
};

// The player's scalar wallet. A struct rather than a component so the
// arithmetic is testable with no actor and no world, exactly like
// FBreakerAttributeAggregator.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerForgeWallet
{
    GENERATED_BODY()

    static constexpr int32 CurrencyCount = static_cast<int32>(EBreakerForgeCurrency::Count);

    // Fixed-size and index-aligned to the enum, so a save written before a
    // currency existed cannot silently map one currency onto another.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<int32> Amounts;

    FBreakerForgeWallet() { Amounts.Init(0, CurrencyCount); }

    int32 Get(EBreakerForgeCurrency Currency) const;
    void Add(EBreakerForgeCurrency Currency, int32 Amount);
    bool CanAfford(const FBreakerForgeCost& Cost) const;
    // Returns false and spends NOTHING when the wallet is short. A partial
    // spend is the failure mode that turns one refused craft into a lost
    // currency bug report.
    bool Spend(const FBreakerForgeCost& Cost);
};

UENUM(BlueprintType)
enum class EBreakerForgeResult : uint8
{
    Success,
    NotAtForge,
    InvalidItem,
    InvalidAffix,
    // The affix is already at T-1, or at the ceiling its rarity allows.
    AtTierCeiling,
    Unaffordable
};

UCLASS()
class RIORSEDGE_API UBreakerForgeLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ---- Currency --------------------------------------------------------
    // What destroying this item pays. Scales on rarity and item level, because
    // an item level 50 Standard and an item level 1 Standard are not the same
    // amount of material. O2 PLACEHOLDER throughout.
    UFUNCTION(BlueprintPure, Category="Items|Forge")
    static FBreakerForgeWallet SalvageValue(const FBreakerItemInstance& Item);

    // ---- Costs, published so the UI states the price before the click -----
    // Tempering an affix from Tier to Tier-1. The currency STEPS with the
    // target tier, which is what makes T0/T-1 a Sigil sink rather than a
    // grind: no amount of Slag reaches them.
    UFUNCTION(BlueprintPure, Category="Items|Forge")
    static FBreakerForgeCost TemperCost(const FBreakerItemInstance& Item, int32 AffixIndex);
    UFUNCTION(BlueprintPure, Category="Items|Forge")
    static FBreakerForgeCost ReforgeCost(const FBreakerItemInstance& Item);
    UFUNCTION(BlueprintPure, Category="Items|Forge")
    static FBreakerForgeCost AttuneCost(const FBreakerItemInstance& Item);

    // The best tier this item may be tempered to. Rarity-capped, exactly as the
    // drop path is: a Standard item stops at T3 no matter how much Sigil the
    // player has, because otherwise crafting would erase rarity's meaning at
    // the same moment this pass gave it one.
    UFUNCTION(BlueprintPure, Category="Items|Forge")
    static int32 TemperCeilingForItem(const FBreakerItemInstance& Item);

    // ---- The three verbs -------------------------------------------------
    // TEMPER: one affix, one tier better, value re-derived at the new tier.
    // This is the T0/T-1 path and the only one.
    UFUNCTION(BlueprintCallable, Category="Items|Forge")
    static EBreakerForgeResult Temper(UPARAM(ref) FBreakerItemInstance& Item, int32 AffixIndex,
        UPARAM(ref) FBreakerForgeWallet& Wallet, bool bIsAtForge);

    // REFORGE: rerolls every affix VALUE within its existing tier band. Cheap
    // agency with no power creep — a T1 roll at the bottom of its band becomes
    // a T1 roll somewhere else in it, and the item's tiers do not move.
    UFUNCTION(BlueprintCallable, Category="Items|Forge")
    static EBreakerForgeResult Reforge(UPARAM(ref) FBreakerItemInstance& Item,
        UPARAM(ref) FBreakerForgeWallet& Wallet, bool bIsAtForge, int32 RandomSeed);

    // ATTUNE: rerolls WHICH affixes the item carries, keeping the count and the
    // tiers. The expensive verb, because it is the one that turns an item with
    // perfect tiers and useless lines into the item you wanted.
    UFUNCTION(BlueprintCallable, Category="Items|Forge")
    static EBreakerForgeResult Attune(UPARAM(ref) FBreakerItemInstance& Item,
        UPARAM(ref) FBreakerForgeWallet& Wallet, bool bIsAtForge, int32 RandomSeed);
};
