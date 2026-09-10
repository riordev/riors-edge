#pragma once

#include "CoreMinimal.h"
#include "Items/BreakerItemTypes.h"

// ---------------------------------------------------------------------------
// WHAT IS IN A CHEST, AND WHERE IT STANDS. World-free.
//
// Owner-asked: "randomly spawning chests the player can open with either
// currency or an item in there weighted more towards lower value stuff". Both
// halves of that sentence are rules rather than taste, so both live here where
// a bare test can read them:
//
//   * EITHER/OR, weighted. A chest pays one thing, not a handful.
//   * TOWARD THE LOWER END. Currency outweighs items, and an item a chest
//     hands over is TEMPERED — stepped down a tier half the time — because a
//     chest costs nothing. There is no fight in front of it and no wallet
//     behind it, and a free reward that pays what a fought one pays makes the
//     fight the worse option.
//
// A NOTE THE OWNER SHOULD HAVE: chest items are NOT inside the pinned
// drops-per-hour band. That band is projected from a KILL RATE, and a chest is
// not a kill, so the measured 134/hour does not see these at all.
// ---------------------------------------------------------------------------
namespace BreakerSupplyChest
{
    // ---- The split. All O2 PLACEHOLDER ----------------------------------
    inline constexpr int32 CurrencyWeight = 62;
    inline constexpr int32 ItemWeight = 38;
    // How often a rolled item is stepped down one rarity tier.
    inline constexpr int32 TemperPercent = 50;
    // How many chests a yard carries.
    inline constexpr int32 ChestsPerYard = 2;

    // One hash, no RNG state: a chest's contents are a pure function of the
    // seed it was configured with, so a test can walk the whole distribution
    // and a capture can be compared with the one before it.
    inline uint32 Mix(int32 Seed, int32 Salt)
    {
        uint32 Value = static_cast<uint32>(Seed) * 2654435761u + static_cast<uint32>(Salt) * 40503u;
        Value ^= Value >> 15;
        Value *= 2246822519u;
        Value ^= Value >> 13;
        return Value;
    }

    inline bool PaysCurrency(int32 Seed)
    {
        return static_cast<int32>(Mix(Seed, 1) % (CurrencyWeight + ItemWeight)) < CurrencyWeight;
    }

    // ONE STEP DOWN, SOMETIMES, and never below Standard. The rarity ladder is
    // walked by NAME rather than by arithmetic on the enumerator: that enum is
    // serialized and append-only, so its numeric order is a storage detail and
    // subtracting one from it is a bug waiting for the next appended entry.
    inline EBreakerItemRarity StepDown(EBreakerItemRarity Rarity)
    {
        switch (Rarity)
        {
            case EBreakerItemRarity::Unwritten:   return EBreakerItemRarity::Aberrant;
            case EBreakerItemRarity::Aberrant:    return EBreakerItemRarity::Exceptional;
            case EBreakerItemRarity::Exceptional: return EBreakerItemRarity::Uncommon;
            case EBreakerItemRarity::Uncommon:    return EBreakerItemRarity::Standard;
            default:                              return EBreakerItemRarity::Standard;
        }
    }

    inline EBreakerItemRarity Temper(EBreakerItemRarity Rolled, int32 Seed)
    {
        return static_cast<int32>(Mix(Seed, 2) % 100) < TemperPercent ? StepDown(Rolled) : Rolled;
    }

    // ---- WHAT A CHEST IS WORTH IN CURRENCY -------------------------------
    // The shipped kill roll pays a TRASH body 0 to 1 Riftglass. That is right
    // for one of forty kills and WRONG for the only thing in a chest: the
    // runtime test caught a chest crediting nothing at all, and a player would
    // have read that as the interaction being broken rather than as a bad roll.
    //
    // So this is a FLOOR AND A MULTIPLE OF THE SHIPPED ROLL rather than a
    // second currency table. A chest is worth about a pocket's worth of bodies,
    // and when the kill number is retuned this follows it instead of drifting
    // away from it. The floor is what guarantees a chest never pays nothing,
    // and it climbs with the yard because a deeper yard is a longer walk.
    // All O2 PLACEHOLDER.
    inline constexpr int32 KillsWorth = 8;
    inline constexpr int32 MinimumRiftglass = 6;

    inline int32 CurrencyPayout(int32 RolledPerKill, int32 AreaLevel)
    {
        const int32 Floor = MinimumRiftglass + FMath::Max(AreaLevel - 1, 0) / 2;
        return FMath::Max(Floor, FMath::Max(RolledPerKill, 0) * KillsWorth);
    }

    // ---- Where one stands ------------------------------------------------
    // A fraction along the yard's own combat band and a lateral offset inside
    // it, both from the seed. RANDOM PER SESSION, not per authored placement:
    // the owner asked for chests that spawn rather than chests that sit, and
    // the difference the player feels is that a route they have walked twice
    // is not identical the third time.
    //
    // The band ENDS are avoided: a chest at fraction 0 is in the entry plaza
    // where nothing else is, and one at 1 is against the far wall. The inset
    // is a fraction of the band rather than a distance, so it survives a yard
    // whose band is retuned.
    inline constexpr float BandInset = 0.12f;

    inline float PlacementFraction(int32 Seed, int32 Index)
    {
        const float Unit = static_cast<float>(Mix(Seed, 11 + Index * 7) & 0xFFFFu) / 65535.0f;
        return BandInset + Unit * (1.0f - 2.0f * BandInset);
    }

    inline float PlacementLateral(int32 Seed, int32 Index, float BandHalfWidthCm)
    {
        const float Unit = static_cast<float>(Mix(Seed, 29 + Index * 13) & 0xFFFFu) / 65535.0f;
        return (Unit * 2.0f - 1.0f) * FMath::Max(BandHalfWidthCm, 0.0f);
    }
}
