#pragma once

#include "CoreMinimal.h"
#include "Classes/BreakerChargeComponent.h"
#include "Classes/BreakerGritComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Classes/BreakerScrapComponent.h"
#include "Data/BreakerStrings.h"
#include "UI/BreakerHUDMath.h"
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
// is what makes the row testable at all, since the drawing itself is not. The
// words come from the string table (O195), a static file read that needs no
// world either.
//
// A RESTING RESOURCE HAS NO STATE WORD. Momentum at rest and Mana in credit
// leave StateWord empty: the fill carries the read, and the word slot is for
// the states that change what the class can do.
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
        FString Label = BreakerStrings::Get(EBreakerStringKey::HudResourceLabel);
        // The confirmation for the centre of vision, never the carrier (§2).
        // Empty for a resource at rest.
        FString StateWord = BreakerStrings::Get(EBreakerStringKey::HudResourceStateNone);
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
        // Fractions along the track where a band edge is marked, so the band
        // is readable off the bar without the word. Empty for a resource with
        // no authored band edges.
        TArray<float> StepMarks;
        // The fill's resting colour. The track's loud states (banked, debt)
        // still own the fill; this is what draws beneath them.
        FLinearColor FillColor = BreakerUI::TextSecondary;
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
        Row.Label = BreakerStrings::Get(EBreakerStringKey::HudResourceMomentumLabel);
        // Momentum is a [0, Max] pool: it has no debt half.
        Row.Fraction = FMath::Clamp(MomentumFraction, 0.0f, 1.0f);
        switch (State)
        {
        case EBreakerMomentumState::Redline:
            Row.StateWord = BreakerStrings::Get(EBreakerStringKey::HudResourceMomentumRedline);
            Row.StateColor = BreakerUI::Orange;
            Row.Track = EResourceTrack::WideBlocks;
            Row.BorderColor = BreakerUI::Orange;
            Row.BorderPixels = 2.0f;
            break;
        case EBreakerMomentumState::Running:
            Row.StateWord = BreakerStrings::Get(EBreakerStringKey::HudResourceMomentumRunning);
            Row.StateColor = BreakerUI::Gold;
            Row.Track = EResourceTrack::Blocks;
            break;
        default:
            // At rest: no word, the low continuous fill is the read.
            Row.StateWord.Reset();
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
        Row.Label = BreakerStrings::Get(EBreakerStringKey::HudResourceManaLabel);
        Row.Track = EResourceTrack::Signed;
        // Caster's cells (O120: one per authored cast) are not drawn: nothing
        // on the Mana component states a cast count or a cell total, and a
        // cell row divided by the pool would be a total nobody authored.
        // Recorded here, not faked with StepMarks.

        const float Floor = FMath::Min(0.0f, OvercastFloor);
        if (Mana < 0.0f)
        {
            // Debt. Harm colour, and the fill runs the other way from zero.
            Row.StateWord = BreakerStrings::Get(EBreakerStringKey::HudResourceManaOvercast);
            Row.StateColor = BreakerUI::Harm;
            Row.BorderColor = BreakerUI::Harm;
            Row.BorderPixels = 2.0f;
            Row.Fraction = Floor < 0.0f ? FMath::Max(Mana / -Floor, -1.0f) : -1.0f;
        }
        else
        {
            // In credit: no word, the upper half of the signed track is the read.
            Row.StateWord.Reset();
            Row.StateColor = BreakerUI::Cyan;
            Row.Fraction = MaxMana > 0.0f ? FMath::Min(Mana / MaxMana, 1.0f) : 0.0f;
        }
        return Row;
    }

    // Gunsmith — Scrap. A ledger: accumulates, never decays, never idles. The
    // three bands reuse Momentum's texture ladder (continuous / blocks / wide
    // blocks) so the "how full is my working capital" read is peripheral, and
    // Surplus — the band the class is built to SPEND OUT OF — owns the loud
    // frame exactly as Redline does.
    inline FResourceRow ResolveScrapRow(float ScrapFraction, EBreakerScrapState State)
    {
        FResourceRow Row;
        Row.bActive = true;
        Row.Label = BreakerStrings::Get(EBreakerStringKey::HudResourceScrapLabel);
        Row.Fraction = FMath::Clamp(ScrapFraction, 0.0f, 1.0f);
        // Scrap is working capital for weapons and deployables, so its fill
        // is the weapon verb's orange (O179).
        Row.FillColor = BreakerUI::Orange;
        switch (State)
        {
        case EBreakerScrapState::Surplus:
            Row.StateWord = BreakerStrings::Get(EBreakerStringKey::HudResourceScrapSurplus);
            Row.StateColor = BreakerUI::Orange;
            Row.Track = EResourceTrack::WideBlocks;
            Row.BorderColor = BreakerUI::Orange;
            Row.BorderPixels = 2.0f;
            break;
        case EBreakerScrapState::Stocked:
            Row.StateWord = BreakerStrings::Get(EBreakerStringKey::HudResourceScrapStocked);
            Row.StateColor = BreakerUI::Gold;
            Row.Track = EResourceTrack::Blocks;
            break;
        default:
            Row.StateWord = BreakerStrings::Get(EBreakerStringKey::HudResourceScrapDry);
            Row.StateColor = BreakerUI::Cyan;
            Row.Track = EResourceTrack::Continuous;
            break;
        }
        return Row;
    }

    // Tank — Grit. A banked state with a lapse timer; IRONCLAD is the band the
    // class holds under pressure and owns the frame.
    inline FResourceRow ResolveGritRow(float GritFraction, EBreakerGritBand Band)
    {
        FResourceRow Row;
        Row.bActive = true;
        Row.Label = BreakerStrings::Get(EBreakerStringKey::HudResourceGritLabel);
        Row.Fraction = FMath::Clamp(GritFraction, 0.0f, 1.0f);
        Row.StepMarks = BreakerHUDMath::GritStepMarks();
        switch (Band)
        {
        case EBreakerGritBand::Ironclad:
            Row.StateWord = BreakerStrings::Get(EBreakerStringKey::HudResourceGritIronclad);
            Row.StateColor = BreakerUI::Orange;
            Row.Track = EResourceTrack::WideBlocks;
            Row.BorderColor = BreakerUI::Orange;
            Row.BorderPixels = 2.0f;
            break;
        case EBreakerGritBand::Braced:
            Row.StateWord = BreakerStrings::Get(EBreakerStringKey::HudResourceGritBraced);
            Row.StateColor = BreakerUI::Gold;
            Row.Track = EResourceTrack::Blocks;
            break;
        default:
            Row.StateWord = BreakerStrings::Get(EBreakerStringKey::HudResourceGritWinded);
            Row.StateColor = BreakerUI::Cyan;
            Row.Track = EResourceTrack::Continuous;
            break;
        }
        return Row;
    }

    // Support — Charge. A bank the class REACHES the top of, spends, and
    // climbs back; RESONANT deliberately starts at three quarters rather than
    // two thirds (the pinned band asymmetry), which the fill length shows
    // without this row needing to know it.
    inline FResourceRow ResolveChargeRow(float ChargeFraction, EBreakerChargeBand Band)
    {
        FResourceRow Row;
        Row.bActive = true;
        Row.Label = BreakerStrings::Get(EBreakerStringKey::HudResourceChargeLabel);
        Row.Fraction = FMath::Clamp(ChargeFraction, 0.0f, 1.0f);
        // The ally segment — the share of Charge an ally's hits paid in — is
        // not drawn: the Charge component states one pool and no per-source
        // split, and there is no party layer to split it by. Recorded here.
        switch (Band)
        {
        case EBreakerChargeBand::Resonant:
            Row.StateWord = BreakerStrings::Get(EBreakerStringKey::HudResourceChargeResonant);
            Row.StateColor = BreakerUI::Orange;
            Row.Track = EResourceTrack::WideBlocks;
            Row.BorderColor = BreakerUI::Orange;
            Row.BorderPixels = 2.0f;
            break;
        case EBreakerChargeBand::Attuned:
            Row.StateWord = BreakerStrings::Get(EBreakerStringKey::HudResourceChargeAttuned);
            Row.StateColor = BreakerUI::Gold;
            Row.Track = EResourceTrack::Blocks;
            break;
        default:
            Row.StateWord = BreakerStrings::Get(EBreakerStringKey::HudResourceChargeCold);
            Row.StateColor = BreakerUI::Cyan;
            Row.Track = EResourceTrack::Continuous;
            break;
        }
        return Row;
    }
}
