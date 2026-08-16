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
// item-derived materials. OWNER RULING (2026-08-16 chat, superseding O12's
// "3-4 tiered currencies"): exactly ONE scalar crafting currency, Riftglass,
// defined below. The slice does not want a crafting game; it wants the top of
// its own tier curve to be reachable and it wants a reason not to vendor
// every drop.
//
// THE LOOP: kill or salvage -> Riftglass -> temper one affix one
// tier better, reforge an item's values, or attune which affixes it carries.
// Three verbs, all deterministic, all pure functions over an item and a wallet.
//
// FORGE-GATED, like the respec. UBreakerProgressionComponent::RespecAtForge
// takes a bIsAtForge flag and refuses away from one; every operation here takes
// the same flag for the same reason, so "the Forge is a place you go" is one
// rule rather than two.

// ---------------------------------------------------------------------------
// THE ONE CURRENCY: RIFTGLASS (owner ruling, 2026-08-16 chat, closing the
// Save-Architecture O12 gap: the Forge economy has exactly one crafting
// currency).
// ---------------------------------------------------------------------------
// The fiction: when a rift is suppressed, the breach edge vitrifies — whatever
// was half-through gets fused into a glassy, faintly humming slag. Breakers
// chip it out of every closed rift and every thing that came through one, and
// the Forge remelts it to rework salvage into gear. It is the material a kill
// leaves behind and the stock a craft consumes, which is why the SAME number
// can honestly be both the drop credit and the cost line. Not coinage: nobody
// mints it, nobody trades it — you pry it out of the world and burn it at the
// Forge.
//
// This replaces the three-denomination Slag/Flux/Sigil wallet. The old
// "currency steps with tier" gate (T0/T-1 were Sigil-only) is expressed in
// PRICE instead: the spike tiers cost boss-scale amounts and their price does
// not scale down with item level, so they stay a chase without needing a
// second ledger. Old saves are folded into Riftglass by
// UBreakerSaveGame::MigrateToCurrent (v3 -> v4) at a stated conversion.
namespace BreakerForge
{
    // The display name, single point of truth for every cost line, chip and
    // salvage preview. All-caps to match the menu's existing label voice.
    inline const TCHAR* CurrencyDisplayName = TEXT("RIFTGLASS");
}

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerForgeCost
{
    GENERATED_BODY()

    // Riftglass. There is exactly one currency, so a cost is just an amount.
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

    // How many denominations the PRE-consolidation wallet had (Slag, Flux,
    // Sigil at indices 0/1/2). Only the save migration cares.
    static constexpr int32 LegacyDenominationCount = 3;

    // The one balance: Riftglass.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Riftglass = 0;

    // LEGACY STORAGE, save versions 3 and earlier: the Slag/Flux/Sigil array,
    // index-aligned to the deleted EBreakerForgeCurrency enum. The property
    // keeps its old name so an old file still deserializes into it;
    // CollapseLegacyDenominations then folds it into Riftglass and empties it.
    // Nothing at runtime reads or writes this.
    UPROPERTY() TArray<int32> Amounts;

    int32 Get() const { return Riftglass; }
    void Add(int32 Amount);
    bool CanAfford(const FBreakerForgeCost& Cost) const;
    // Returns false and spends NOTHING when the wallet is short. A partial
    // spend is the failure mode that turns one refused craft into a lost
    // currency bug report.
    bool Spend(const FBreakerForgeCost& Cost);

    // Folds a pre-v4 Slag/Flux/Sigil balance into Riftglass and clears the
    // legacy array. Total value preserved at the stated conversion (see the
    // implementation); idempotent, so migrating twice cannot double a balance.
    // Returns true if anything changed.
    bool CollapseLegacyDenominations();
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
    // Tempering an affix from Tier to Tier-1. The price climbs steeply with
    // the target tier, and the spike tiers (T0/T-1) are priced flat at boss
    // scale — see the ladder table at the cost site in the .cpp.
    UFUNCTION(BlueprintPure, Category="Items|Forge")
    static FBreakerForgeCost TemperCost(const FBreakerItemInstance& Item, int32 AffixIndex);
    UFUNCTION(BlueprintPure, Category="Items|Forge")
    static FBreakerForgeCost ReforgeCost(const FBreakerItemInstance& Item);
    UFUNCTION(BlueprintPure, Category="Items|Forge")
    static FBreakerForgeCost AttuneCost(const FBreakerItemInstance& Item);

    // The best tier this item may be tempered to. Rarity-capped, exactly as the
    // drop path is: a Standard item stops at its cap no matter how much
    // Riftglass the player has, because otherwise crafting would erase rarity's
    // meaning at the same moment this pass gave it one.
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
