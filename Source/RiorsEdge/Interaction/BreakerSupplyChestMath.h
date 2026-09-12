#pragma once

#include "CoreMinimal.h"
#include "Game/BreakerZoneBuilder.h"

// ---------------------------------------------------------------------------
// WHAT IS IN A CHEST, AND WHERE IT STANDS. World-free.
//
// O275. A supply chest stands BEHIND FULL-HEIGHT COVER OR INSIDE A BAY, never
// on open lane ground: the seed picks the SITES, not the coordinates. And it
// always pays BOTH — one item at the completion floor and the currency floor.
// A chest is the reward for leaving the lane, and a reward that is sometimes
// six Riftglass and nothing else does not pay for the walk.
//
// The two rules live here rather than in the actor or the spawner so a bare
// test can read them:
//
//   * A SITE IS A PLACE THE YARD ALREADY HAS. Every blk_full_* piece offers
//     its far face — the side away from the yard's centreline — and every
//     flr_<tag>_bay slab offers its interior. Nothing is authored; the sites
//     are read off the composed yard, so a re-composed yard moves its chests
//     with it.
//   * THE SEED PICKS. Random per session, not per authored placement: the
//     owner asked for chests that SPAWN rather than chests that sit, and the
//     difference the player feels is that a route walked twice is not
//     identical the third time. The seed chooses WHICH sites; it never
//     invents one.
//
// A NOTE THE OWNER SHOULD HAVE: chest items are NOT inside the pinned
// drops-per-hour band. That band is projected from a KILL RATE, and a chest is
// not a kill, so the measured 134/hour does not see these at all.
// ---------------------------------------------------------------------------
namespace BreakerSupplyChest
{
    // How many chests a yard carries. O2 PLACEHOLDER.
    inline constexpr int32 ChestsPerYard = 2;

    // The level a chest's contents roll at is the yard's own, never below the
    // ladder's first rung.
    inline int32 ChestItemLevel(int32 AreaLevel)
    {
        return FMath::Max(1, AreaLevel);
    }

    // One hash, no RNG state: a chest's contents and its site are a pure
    // function of the seed it was configured with, so a test can walk the
    // whole seed space and a capture can be compared with the one before it.
    inline uint32 Mix(int32 Seed, int32 Salt)
    {
        uint32 Value = static_cast<uint32>(Seed) * 2654435761u + static_cast<uint32>(Salt) * 40503u;
        Value ^= Value >> 15;
        Value *= 2246822519u;
        Value ^= Value >> 13;
        return Value;
    }

    // ---- WHAT A CHEST IS WORTH IN CURRENCY -------------------------------
    // The shipped kill roll pays a TRASH body 0 to 1 Riftglass. That is right
    // for one of forty kills and WRONG for a chest: the runtime test caught a
    // chest crediting nothing at all, and a player would have read that as the
    // interaction being broken rather than as a bad roll.
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
    // A SITE: the ground point a chest is offered, and the piece it stands
    // behind or inside. The location is on the piece's own ground; the spawner
    // still traces the floor and pushes off bodies, because a site is where
    // the yard SAYS a chest fits and the world is what proves it.
    struct FBreakerChestSite
    {
        FVector Location = FVector::ZeroVector;
        FName Cover;
    };

    // Room between the cover's far face and the chest's capsule, so the chest
    // reads as standing behind the cover rather than welded to it. O2 PLACEHOLDER.
    inline constexpr float CoverMarginCm = 60.0f;
    // How far a bay's site is pulled back from its mouth toward the back wall:
    // out of the doorway, clear of the barrels the composer stands in it, and
    // short of the crate row against the back. O2 PLACEHOLDER.
    inline constexpr float BayPullbackCm = 200.0f;
    // Where the spawner's floor trace STARTS above a site's ground. A bay has a
    // roof six metres up, so a trace dropped from thirty metres lands a chest
    // on top of the bay; this starts under the roof and above the bay's stock
    // (crates are 130 cm). O2 PLACEHOLDER.
    inline constexpr float SiteTraceHeightCm = 250.0f;

    inline bool IsFullCover(const FString& PieceName)
    {
        return PieceName.StartsWith(TEXT("blk_full_"));
    }

    inline bool IsBay(const FString& PieceName)
    {
        return PieceName.StartsWith(TEXT("flr_")) && PieceName.EndsWith(TEXT("_bay"));
    }

    // Half of a piece's footprint along an axis. Imported bounds are baked
    // world placement, so the extent is read along the axis rather than
    // assumed to be the box's X.
    inline float HalfExtentAlong(const FVector& Extent, const FVector& Axis)
    {
        return FMath::Abs(Axis.X) * Extent.X + FMath::Abs(Axis.Y) * Extent.Y;
    }

    // The yard's lateral axis, from its frame. Held here so the spawner and
    // the test cannot disagree about which way "away from the lane" is.
    inline FVector LateralAxis(const FVector& Forward)
    {
        return FVector::CrossProduct(FVector::UpVector, Forward);
    }

    // Which side of the centreline a point stands on: +1 or -1, never 0, so a
    // piece exactly on the lane still has a far side.
    inline float LateralSign(const FVector& Point, const FVector& YardOrigin, const FVector& Forward)
    {
        return FVector::DotProduct(Point - YardOrigin, LateralAxis(Forward)) >= 0.0f ? 1.0f : -1.0f;
    }

    // EVERY SITE THE YARD OFFERS, in piece order. Pieces are the yard's own
    // (the caller has already answered which yard each stands in); the frame
    // is the yard's anchor and its forward, which is the centreline.
    inline void CollectChestSites(const TArray<FBreakerZonePiece>& YardPieces, const FVector& YardOrigin,
        const FVector& Forward, float ChestRadiusCm, TArray<FBreakerChestSite>& OutSites)
    {
        const FVector Right = LateralAxis(Forward);
        for (const FBreakerZonePiece& Piece : YardPieces)
        {
            const float Sign = LateralSign(Piece.Origin, YardOrigin, Forward);
            const float GroundZ = Piece.Origin.Z - Piece.Extent.Z;
            if (IsFullCover(Piece.Name))
            {
                // The far face, plus the chest's own radius and a margin: the
                // cover stands between the lane and the chest.
                const float Away = HalfExtentAlong(Piece.Extent, Right)
                    + FMath::Max(ChestRadiusCm, 0.0f) + CoverMarginCm;
                FBreakerChestSite& Site = OutSites.AddDefaulted_GetRef();
                Site.Location = Piece.Origin + Right * (Sign * Away);
                Site.Location.Z = GroundZ;
                Site.Cover = FName(*Piece.Name);
            }
            else if (IsBay(Piece.Name))
            {
                // The interior, pulled back from the mouth. The mouth faces
                // the lane, so "back" is the same sign as "away".
                FBreakerChestSite& Site = OutSites.AddDefaulted_GetRef();
                Site.Location = Piece.Origin + Right * (Sign * BayPullbackCm);
                Site.Location.Z = GroundZ;
                Site.Cover = FName(*Piece.Name);
            }
        }
    }

    // THE SEED PICKS Count DISTINCT SITES. A seeded partial shuffle, so no
    // site is offered twice and a yard with fewer sites than asked simply
    // offers what it has — the caller says so, this does not invent one.
    inline TArray<FBreakerChestSite> PickChestSites(const TArray<FBreakerChestSite>& Sites, int32 Seed, int32 Count)
    {
        TArray<int32> Order;
        Order.Reserve(Sites.Num());
        for (int32 Index = 0; Index < Sites.Num(); ++Index) Order.Add(Index);
        const int32 Picks = FMath::Clamp(Count, 0, Sites.Num());
        TArray<FBreakerChestSite> Picked;
        Picked.Reserve(Picks);
        for (int32 Slot = 0; Slot < Picks; ++Slot)
        {
            const int32 Remaining = Order.Num() - Slot;
            const int32 Swap = Slot + static_cast<int32>(Mix(Seed, 11 + Slot * 7) % static_cast<uint32>(Remaining));
            if (Swap != Slot) Order.Swap(Slot, Swap);
            Picked.Add(Sites[Order[Slot]]);
        }
        return Picked;
    }
}
