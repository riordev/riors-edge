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

    // THE RAMP IS APPLIED AS A DELTA FROM ITS OWN 100% ENTRY, not as an
    // absolute body colour, and that is a DEVIATION from the pack worth
    // stating plainly. Measured (CIE dE76, sRGB/D65, the same method that
    // killed the pack's rank claim): shipping the twenty colours absolutely
    // collapses the three families' minimum separation from 12.1 to 0.0 — a
    // Vestige, an Altered and a Lattice become literally the same body colour,
    // which deletes the one thing O24 spends colour on. At three quarters
    // weight it is 2.9, which is the just-noticeable difference; at crowd
    // distance that is gone too.
    //
    // The delta form keeps every axis the pack wanted and costs nothing:
    // health travels 44.7 / 61.7 / 51.9 / 44.7 dE76 across the four rank rows
    // against the authored 44.1 / 52.9 / 54.7 / 40.2, with the same step
    // growth toward death (Trash 6.6 -> 12.6 against the authored 5.9 -> 13.1);
    // family separation is UNCHANGED at full health and still 8.9 at ten
    // percent; and the body at full health is bit-identical to what it is
    // today, so nothing regresses on an undamaged enemy.
    //
    // What the delta form drops is the ramp's RANK offsets, which is the
    // ruling and not an accident: the pack's claim that rank separates by
    // value band does not survive measurement (minimum pairwise L* separation
    // 0.99 at full health, 0.59 at a quarter — a full-health Champion and a
    // full-health trash mob are the same lightness), and rank already has four
    // carriers that work. Rank keeps its blend layer above and nothing else.
    // One line changes this back if the crowd probe disagrees.
    inline FLinearColor HealthRampOffset(EBreakerMonsterRank Rank, float Fraction)
    {
        return SampleHealthRamp(Rank, Fraction) - HealthRampRow(Rank)[0];
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

        FLinearColor Body = FMath::Lerp(State.FamilyPaint, RankHueFor(State.Rank), RankBlendFor(State.Rank));
        if (State.bHealthRamp) Body += HealthRampOffset(State.Rank, State.HealthFraction);
        // THE BODY NEVER GOES OVERBRIGHT; ONLY THE REACTION DOES. Unclamped,
        // an Elite at ten percent pushes red past 1.0 and blooms, which is the
        // hit flash's whole vocabulary being spent by a health value. The
        // flash constants above are deliberately over one and stay that way.
        return FLinearColor(
            FMath::Clamp(Body.R, 0.0f, 1.0f),
            FMath::Clamp(Body.G, 0.0f, 1.0f),
            FMath::Clamp(Body.B, 0.0f, 1.0f), 1.0f);
    }
}
