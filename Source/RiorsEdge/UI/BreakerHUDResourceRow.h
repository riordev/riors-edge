#pragma once

#include "CoreMinimal.h"
#include "Classes/BreakerMomentumComponent.h"
#include "UI/BreakerUIStyle.h"

// ---------------------------------------------------------------------------
// The class-resource row of the combat cluster (UI-HUD-Spec §2).
//
// The row is deliberately generic: "every class drops its own resource into
// this slot, so the row has to hold a different label and range without
// regrowing." So the HUD never asks a class component what it is; it asks for
// a resolved description of the row — label, signed fraction, state word,
// state colour and the track's texture treatment — and draws that. Adding
// Gunsmith/Tank/Support later is one more Resolve* function and one more
// branch in the resolver, never a new drawing path.
//
// Everything in this header is pure: no world, no components, no actors. That
// is what makes the row testable at all, since the drawing itself is not.
// ---------------------------------------------------------------------------
namespace BreakerHUD
{
    // How the fill is textured inside the fixed 12px track. §2 fixes the track
    // height and the two notches; only the texture changes per class/state.
    enum class EResourceTrack : uint8
    {
        // No resource for this class: the track is drawn empty, never omitted,
        // so the cluster does not change height between classes.
        Empty,
        // One continuous low bar (Swift Settled).
        Continuous,
        // 8px chevron-cut blocks (Swift Running).
        Blocks,
        // 14px chevron-cut blocks (Swift Redline).
        WideBlocks,
        // A signed channel: a zero baseline across the middle of the track,
        // credit filling the upper half and debt filling the lower half from
        // the same left origin. Caster's Mana, whose Overcast bank is negative.
        Signed
    };

    struct FResourceRow
    {
        // Uppercase, drawn at caption weight in the fixed left column.
        FString Label = TEXT("RESOURCE");
        // The confirmation for the centre of vision, never the carrier (§2).
        FString StateWord = TEXT("NO RESOURCE");
        // SIGNED. [0,1] is credit against the resource's maximum; [-1,0) is
        // debt against the resource's own negative floor. A resource that
        // cannot go negative never returns a negative value here.
        float Fraction = 0.0f;
        FLinearColor StateColor = BreakerUI::TextDisabled;
        EResourceTrack Track = EResourceTrack::Empty;
        // The track's own border. Redline widens it to 2px orange; Overcast
        // widens it to 2px harm on the same argument — the loudest state of a
        // resource owns its frame.
        FLinearColor BorderColor = BreakerUI::BorderEmphasis;
        float BorderPixels = 1.0f;
        // False when no class resource applies. The label/state word still
        // render (in text/disabled) so the row is never a blank strip.
        bool bActive = false;
    };

    // No class resource: every class that has not been wired yet.
    inline FResourceRow ResolveEmptyResourceRow()
    {
        return FResourceRow();
    }

    // Swift — Momentum. A three-state machine separated by colour, fill height
    // and segment count (§2's table), never by reading the word.
    inline FResourceRow ResolveMomentumRow(float MomentumFraction, EBreakerMomentumState State)
    {
        FResourceRow Row;
        Row.bActive = true;
        Row.Label = TEXT("MOMENTUM");
        // Momentum is a [0, Max] pool: it has no debt half.
        Row.Fraction = FMath::Clamp(MomentumFraction, 0.0f, 1.0f);
        switch (State)
        {
        case EBreakerMomentumState::Redline:
            Row.StateWord = TEXT("REDLINE");
            Row.StateColor = BreakerUI::Orange;
            Row.Track = EResourceTrack::WideBlocks;
            Row.BorderColor = BreakerUI::Orange;
            Row.BorderPixels = 2.0f;
            break;
        case EBreakerMomentumState::Running:
            Row.StateWord = TEXT("RUNNING");
            Row.StateColor = BreakerUI::Gold;
            Row.Track = EResourceTrack::Blocks;
            break;
        default:
            Row.StateWord = TEXT("SETTLED");
            Row.StateColor = BreakerUI::Cyan;
            Row.Track = EResourceTrack::Continuous;
            break;
        }
        return Row;
    }

    // Caster — Mana. A bank, not a state machine: it fills and it is spent,
    // and its one state is Overcast, the debt below zero (Class-Kits §2.1).
    //
    // The fraction is signed on purpose. Credit divides by the maximum;
    // DEBT DIVIDES BY THE OVERCAST FLOOR, not by the maximum, because the
    // question the bar answers while negative is "how much rope is left", and
    // the rope is the floor. -20 of a -20 floor is a full bar of debt whether
    // the Caster's pool is 100 or 400.
    //
    // OvercastFloor is negative (or zero when the class has no debt at all).
    inline FResourceRow ResolveManaRow(float Mana, float MaxMana, float OvercastFloor)
    {
        FResourceRow Row;
        Row.bActive = true;
        Row.Label = TEXT("MANA");
        Row.Track = EResourceTrack::Signed;

        const float Floor = FMath::Min(0.0f, OvercastFloor);
        if (Mana < 0.0f)
        {
            // Debt. Harm colour, and the fill runs the other way from zero.
            Row.StateWord = TEXT("OVERCAST");
            Row.StateColor = BreakerUI::Harm;
            Row.BorderColor = BreakerUI::Harm;
            Row.BorderPixels = 2.0f;
            Row.Fraction = Floor < 0.0f ? FMath::Max(Mana / -Floor, -1.0f) : -1.0f;
        }
        else
        {
            Row.StateWord = TEXT("BANKED");
            Row.StateColor = BreakerUI::Cyan;
            Row.Fraction = MaxMana > 0.0f ? FMath::Min(Mana / MaxMana, 1.0f) : 0.0f;
        }
        return Row;
    }
}
