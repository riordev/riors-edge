#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerMonsterChassis.h"

// ---------------------------------------------------------------------------
// THE BODY'S COLOUR, RESOLVED IN ONE PLACE (O128).
//
// `Color` is the only material parameter this project writes — one name, no
// scalars, thirty-nine call sites — and on the enemy BODY three systems were
// writing it: the family paint, the rank blend and the hit flash. Two of them
// kept a private capture-and-restore cache (RankBaseColors, BodyMaterialBases)
// over the same parameter, and two caches over one parameter is a race with
// nothing to arbitrate it. The reachable failure: a chassis pass schedules the
// rank repaint for next tick, a flash lands first, the rank layer's capture
// reads the FLASH colour as the family paint, and from then on every demotion
// restores the body to white. The reaction layer had already learned this and
// guards itself; the rank layer was written afterwards without the guard.
//
// The fix is not a third guard. Capture-and-restore is what makes ordering
// matter at all — three layers is six orderings, four is twenty-four — so the
// layers COMPOSE FORWARD instead:
//
//     family paint  ->  rank blend  ->  health ramp  ->  reaction
//
// Each layer contributes a value to a pure function of STATE. Nothing reads
// the parameter back (there is not one GetVectorParameterValue left on the
// body path), nothing snapshots, and every ordering question is answered by
// construction: the resting colour is whatever Resolve says it is, computed
// from scratch, whenever any layer moves. A flash cannot become a base
// because no layer has a base to be.
//
// SCOPE: the six body parts an enemy registers, and the dummy's one. The
// Warden's shield, the Skirmisher's muzzle and insignia, the Boss's apparatus
// and the modifier halo each already have exactly one writer and are not
// touched by this.
//
// Pure: no AActor, no UWorld, no subsystem. The rank enum is the only import.
// ---------------------------------------------------------------------------
namespace BreakerBodyPaint
{
    // --- Layer 1: the family paints ----------------------------------------
    // Authored once here rather than at each class's material call, so the
    // colour a class DECLARES and the colour its constructor paints cannot
    // drift apart — they are the same symbol. O2 PLACEHOLDER, all of them;
    // the hues themselves are O24's reservation and the reasoning stays at
    // each class.
    const FLinearColor VestigeFamilyPaint(0.35f, 0.32f, 0.42f);
    const FLinearColor AlteredFamilyPaint(0x4A / 255.0f, 0x50 / 255.0f, 0x49 / 255.0f);   // #4A5049
    const FLinearColor LatticeFamilyPaint(0.20f, 0.27f, 0.19f);
    const FLinearColor DummyFamilyPaint(0.82f, 0.84f, 0.88f);

    // --- Layer 4: the reaction ---------------------------------------------
    // All O2 PLACEHOLDER, tuned by eye against capture stills; moved here from
    // the reaction component so Resolve is the whole of the colour.
    const FLinearColor FlashColor(1.35f, 1.30f, 1.25f);
    const FLinearColor FlashWeakPointColor(1.60f, 1.15f, 0.35f);
    // Ash: near-black with a breath of the body's violet, so the corpse reads
    // as burnt out rather than as painted black.
    const FLinearColor DeathAshColor(0.05f, 0.045f, 0.06f);

    enum class EReaction : uint8
    {
        // No reaction in flight. Layers 1-3 are what you see.
        Rest,
        // The one-blink hit pulse, and beat one of the death beat: both paint
        // the flash colour outright, which is why they share a case.
        Flash,
        // Beat two: the flash colour burning down to ash over ReactionAlpha.
        DeathCrumple
    };

    // --- Layers 2 and 3: the authored tables --------------------------------
    // RANK, O2 PLACEHOLDER. Elite is the reward gold; ModifierBearing the
    // ultimate violet's cooler cousin. Trash keeps its family paint untouched
    // and the Boss subclass owns its whole identity, so both blend at zero —
    // which is also what makes a DEMOTION free: rank Trash resolves to the
    // family paint with no restore step anywhere.
    inline FLinearColor RankHueFor(EBreakerMonsterRank Rank)
    {
        switch (Rank)
        {
        case EBreakerMonsterRank::Elite:           return FLinearColor(1.0f, 0.72f, 0.25f);
        case EBreakerMonsterRank::ModifierBearing: return FLinearColor(0.62f, 0.38f, 0.95f);
        default:                                   return FLinearColor::White;
        }
    }
    inline float RankBlendFor(EBreakerMonsterRank Rank)
    {
        switch (Rank)
        {
        case EBreakerMonsterRank::Elite:           return 0.45f;   // O2 PLACEHOLDER
        case EBreakerMonsterRank::ModifierBearing: return 0.40f;   // O2 PLACEHOLDER
        default:                                   return 0.0f;
        }
    }

    // HEALTH, twenty authored colours from the readability pack, O2
    // PLACEHOLDER, at the five authored stops below. Expressed as hex bytes
    // over 255 — the same direct 0-1 convention every other colour in this
    // project uses, gamma left alone.
    constexpr int32 HealthStopCount = 5;
    inline const float* HealthStops()
    {
        static const float Stops[HealthStopCount] = { 1.00f, 0.75f, 0.50f, 0.25f, 0.10f };
        return Stops;
    }
    inline const FLinearColor* HealthRampRow(EBreakerMonsterRank Rank)
    {
        auto Hex = [](int32 R, int32 G, int32 B)
        { return FLinearColor(R / 255.0f, G / 255.0f, B / 255.0f); };
        static const FLinearColor Trash[HealthStopCount] = {
            Hex(0x35,0x40,0x5C), Hex(0x46,0x42,0x5C), Hex(0x5A,0x3F,0x52), Hex(0x6E,0x3B,0x42), Hex(0x7E,0x33,0x36) };
        static const FLinearColor Elite[HealthStopCount] = {
            Hex(0x2E,0x5C,0x7C), Hex(0x41,0x60,0x78), Hex(0x5A,0x54,0x68), Hex(0x74,0x45,0x5A), Hex(0x8C,0x3A,0x46) };
        static const FLinearColor Champion[HealthStopCount] = {
            Hex(0x4A,0x35,0x80), Hex(0x5A,0x3A,0x7A), Hex(0x70,0x38,0x6B), Hex(0x88,0x34,0x59), Hex(0xA0,0x2E,0x4A) };
        static const FLinearColor Boss[HealthStopCount] = {
            Hex(0x8A,0x5A,0x1E), Hex(0x92,0x51,0x1F), Hex(0x9C,0x44,0x25), Hex(0xA6,0x36,0x2B), Hex(0xB0,0x2A,0x2E) };
        switch (Rank)
        {
        case EBreakerMonsterRank::Elite:           return Elite;
        case EBreakerMonsterRank::ModifierBearing: return Champion;
        case EBreakerMonsterRank::Boss:            return Boss;
        default:                                   return Trash;
        }
    }

    // The authored row, sampled at a health fraction. Piecewise-linear between
    // the five stops, clamped flat outside them: above 1.0 is the 100% colour
    // and below 0.10 is the 10% colour, so overhealing and the last sliver
    // both hold still rather than extrapolating off the end of the table.
    inline FLinearColor SampleHealthRamp(EBreakerMonsterRank Rank, float Fraction)
    {
        const FLinearColor* Row = HealthRampRow(Rank);
        const float* Stops = HealthStops();
        if (Fraction >= Stops[0]) return Row[0];
        for (int32 i = 0; i < HealthStopCount - 1; ++i)
        {
            if (Fraction >= Stops[i + 1])
            {
                const float Span = Stops[i] - Stops[i + 1];
                const float Alpha = Span > KINDA_SMALL_NUMBER ? (Stops[i] - Fraction) / Span : 0.0f;
                return FMath::Lerp(Row[i], Row[i + 1], Alpha);
            }
        }
        return Row[HealthStopCount - 1];
    }

    // THE RAMP IS A DELTA FROM ITS OWN 100% ENTRY, APPLIED IN THE DISPLAY
    // DOMAIN THE PACK AUTHORED IN (O146). Two deviations, both measured, and
    // the second exists because the first was got wrong once already.
    //
    // METHOD, stated because every figure below depends on it and the last
    // set was reported without it (O145): FLinearColor is LINEAR, so the
    // perceived colour is its sRGB ENCODING. Figures are CIE dE76 on the
    // encoded value, D65, with the rank blend applied and the gamut clamp
    // applied, per family. There is no single per-rank number — the delta
    // form starts from a family base, so the delivered travel differs per
    // family by construction, and a per-rank average is the one format that
    // hides it.
    //
    // WHY A DELTA AND NOT THE PACK'S ABSOLUTES. Shipping the twenty colours
    // as the body colour collapses the three families to ONE: minimum family
    // separation goes 11.4 -> 0.0 dE76, and O24 spends colour on exactly that
    // read. The delta form holds 11.4 at full health and 10.2 at ten percent.
    //
    // WHY THE DISPLAY DOMAIN. The pack's hexes are display colours — someone
    // picked them looking at a screen. Added to a linear value they deliver a
    // travel that depends on where the family base happens to sit, and the
    // spread was enormous: 27.7 dE76 for a Vestige Champion against 58.5 for
    // a Lattice Boss, with Champion the worst rank in the game to read and
    // Vestige trash tied with it. Adding the SAME authored offset in the
    // domain it was authored in makes the travel nearly base-independent:
    //
    //     rank        Vestige  Altered  Lattice     (authored row, for scale)
    //     Trash          39.5     40.9     41.8          32.8
    //     Elite          38.8     41.4     45.1          41.1
    //     Champion       43.7     46.6     48.8          34.5
    //     Boss           46.6     48.2     49.9          43.3
    //
    // Spread across all twelve falls from 30.8 to 11.1 and the worst case
    // rises from 27.7 to 38.8. NOTHING WAS RETUNED TO GET THAT: the twenty
    // hexes, both rank hues and both blend weights are exactly as authored.
    // Champion was never a blend problem; it was this.
    //
    // WHAT IT COSTS. Five of sixty (family, rank, stop) triples now leave the
    // gamut and clamp, all at the ten-percent stop, worst overshoot 0.191 on
    // Elite's red — Elite's gold base sits high in red and the ramp's red
    // push runs out of headroom there. Elite still travels 38.8-45.1.
    //
    // WHAT IS STILL WRONG, recorded rather than fixed: the delta carries
    // MAGNITUDE, not hue direction, so a rank whose authored row barely moves
    // blue lands somewhere else when added to a blue-ish family. A Vestige
    // Boss dies MAGENTA (#A099AD -> #C669BD), not red. The readability metric
    // is uniform now; the look question is the owner's.
    //
    // The pack's RANK claim does not ship, and the correct space kills it
    // harder than the wrong one did: minimum pairwise rank L* separation is
    // 0.31 at full health and 0.02 at three quarters, where Elite and Boss are
    // the same lightness. Rank keeps its blend layer and its four other
    // carriers.

    // Linear <-> display. Written out rather than taken from FColor because
    // the round trip through 8 bits would quantise the ramp.
    inline float EncodeChannel(float Linear)
    {
        const float C = FMath::Clamp(Linear, 0.0f, 1.0f);
        return C <= 0.0031308f ? 12.92f * C : 1.055f * FMath::Pow(C, 1.0f / 2.4f) - 0.055f;
    }
    inline float DecodeChannel(float Display)
    {
        const float C = FMath::Clamp(Display, 0.0f, 1.0f);
        return C <= 0.04045f ? C / 12.92f : FMath::Pow((C + 0.055f) / 1.055f, 2.4f);
    }

    // The authored offset, in display units. Zero at full health, exactly.
    inline FLinearColor HealthRampOffset(EBreakerMonsterRank Rank, float Fraction)
    {
        return SampleHealthRamp(Rank, Fraction) - HealthRampRow(Rank)[0];
    }

    // Travel Body along that offset in the display domain and come back.
    // Zero offset returns Body untouched rather than round-tripping it, so an
    // undamaged body is bit-identical to what it would be with no ramp at all
    // — the property the no-regression test pins.
    inline FLinearColor ApplyHealthRamp(const FLinearColor& Body, EBreakerMonsterRank Rank, float Fraction)
    {
        const FLinearColor Offset = HealthRampOffset(Rank, Fraction);
        if (Offset.R == 0.0f && Offset.G == 0.0f && Offset.B == 0.0f) return Body;
        return FLinearColor(
            DecodeChannel(EncodeChannel(Body.R) + Offset.R),
            DecodeChannel(EncodeChannel(Body.G) + Offset.G),
            DecodeChannel(EncodeChannel(Body.B) + Offset.B), 1.0f);
    }

    // --- The state, and the whole of the colour ----------------------------
    struct FState
    {
        // Layer 1. DECLARED by the owning class, never read back off the
        // material. That is the property that makes the race unreachable
        // rather than merely guarded.
        FLinearColor FamilyPaint = VestigeFamilyPaint;
        EBreakerMonsterRank Rank = EBreakerMonsterRank::Trash;
        float HealthFraction = 1.0f;
        // The dummy has no health axis and no rank; it opts out rather than
        // being handed a fraction that never moves.
        bool bHealthRamp = false;
        EReaction Reaction = EReaction::Rest;
        float ReactionAlpha = 0.0f;
        bool bReactionWeakPoint = false;
    };

    inline FLinearColor Resolve(const FState& State)
    {
        const FLinearColor Flash = State.bReactionWeakPoint ? FlashWeakPointColor : FlashColor;
        // The reaction OCCLUDES rather than tints: a flash is white whatever
        // the body was, which is what it was always doing, and what makes it
        // legible. It reads the layers below only to be replaced by them when
        // it ends.
        if (State.Reaction == EReaction::Flash) return Flash;
        if (State.Reaction == EReaction::DeathCrumple)
        {
            return FMath::Lerp(Flash, DeathAshColor, FMath::Clamp(State.ReactionAlpha, 0.0f, 1.0f));
        }

        // The rank blend stays in LINEAR: it carries no authored display
        // number, it was tuned by eye against what shipped, and moving it
        // would be a retune wearing a bug fix's clothes. Only the ramp
        // carries the pack's display-domain values, and only the ramp moves.
        FLinearColor Body = FMath::Lerp(State.FamilyPaint, RankHueFor(State.Rank), RankBlendFor(State.Rank));
        if (State.bHealthRamp) Body = ApplyHealthRamp(Body, State.Rank, State.HealthFraction);
        // THE BODY NEVER GOES OVERBRIGHT; ONLY THE REACTION DOES. Spending
        // the hit flash's vocabulary on a health value would cost the flash
        // its meaning. The clamp is inside DecodeChannel; this is the belt on
        // the layers that never travel through it.
        return FLinearColor(
            FMath::Clamp(Body.R, 0.0f, 1.0f),
            FMath::Clamp(Body.G, 0.0f, 1.0f),
            FMath::Clamp(Body.B, 0.0f, 1.0f), 1.0f);
    }
}
