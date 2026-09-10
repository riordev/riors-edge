#pragma once

#include "CoreMinimal.h"
#include "Data/BreakerStrings.h"
#include "Progression/BreakerProgressionTypes.h"
#include "UI/BreakerUIStyle.h"

// ---------------------------------------------------------------------------
// WHAT AN ABILITY COSTS, AS WORDS.
//
// Owner, playtest 2026-09-10: "spell costs actually appearing in the skill
// menu". They were not hidden or mis-drawn: the abilities screen has never
// printed a price of any kind. Every definition has carried ResourceCost,
// CooldownSeconds and CastTimeSeconds since it was written, Data/abilities.json
// authors real values for all of them, and the only thing on any screen that
// ever mentioned a cost was the character sheet's RESOURCE COST multiplier —
// which is a modifier, not a price. A player choosing between two abilities
// could not see what either one takes out of the pool they are choosing for.
//
// PURE. This authors no number: it reads the three the definition already
// carries and turns them into the three text runs a row draws. What it does
// decide is the wording, and one piece of that is a ruled distinction rather
// than a formatting choice (Class-Kits: "Mana IS the cooldown. An empty
// cooldown means cost-gated, and the HUD must distinguish that from a cooldown
// of zero"). A bare "0S" would say the opposite of what it means.
// ---------------------------------------------------------------------------
namespace BreakerAbilityCost
{
    // The pool the price is paid out of, from the same string table the HUD's
    // resource rail reads, so a rename lands in both places at once. None for a
    // classless definition, which prints its number without a pool name rather
    // than guessing at one.
    inline FString ResourceWordFor(EBreakerClassId ClassId)
    {
        switch (ClassId)
        {
        case EBreakerClassId::Swift:    return BreakerStrings::Get(EBreakerStringKey::HudResourceMomentumLabel);
        case EBreakerClassId::Caster:   return BreakerStrings::Get(EBreakerStringKey::HudResourceManaLabel);
        case EBreakerClassId::Gunsmith: return BreakerStrings::Get(EBreakerStringKey::HudResourceScrapLabel);
        case EBreakerClassId::Tank:     return BreakerStrings::Get(EBreakerStringKey::HudResourceGritLabel);
        case EBreakerClassId::Support:  return BreakerStrings::Get(EBreakerStringKey::HudResourceChargeLabel);
        default:                        return FString();
        }
    }

    // Seconds as a player reads them: whole numbers stay whole, and anything
    // finer keeps one decimal. 0.75 s of wind-up is a real difference from
    // 1 s and "1S" would be a lie; "12.0S" for a flat twelve is just noise.
    inline FString FormatSeconds(float Seconds)
    {
        const float Rounded = FMath::RoundToFloat(Seconds * 10.0f) / 10.0f;
        return FMath::IsNearlyEqual(Rounded, FMath::RoundToFloat(Rounded))
            ? FString::Printf(TEXT("%.0fS"), Rounded)
            : FString::Printf(TEXT("%.1fS"), Rounded);
    }

    // Three runs rather than one string, because the row draws them in three
    // weights: the price is the thing the player is deciding with, the gate and
    // the wind-up are context. A run left empty is not drawn at all.
    struct FCostLine
    {
        // "35 MANA". Empty when the ability takes nothing from the pool.
        FString CostText;
        // "12S COOLDOWN", or the ruled distinction when there is no cooldown.
        FString GateText;
        // "0.6S CAST" (O266). Empty when the ability lands on the keypress,
        // which is what most of them still do.
        FString CastText;
    };

    inline FCostLine Compose(float ResourceCost, float CooldownSeconds, float CastTimeSeconds,
        const FString& ResourceWord)
    {
        FCostLine Line;
        if (ResourceCost > 0.0f)
        {
            const FString Amount = BreakerUI::FormatTicker(ResourceCost);
            Line.CostText = ResourceWord.IsEmpty() ? Amount : Amount + TEXT(" ") + ResourceWord;
        }
        if (CooldownSeconds > 0.0f)
        {
            Line.GateText = FormatSeconds(CooldownSeconds) + TEXT(" COOLDOWN");
        }
        else
        {
            // THE RULED DISTINCTION. No cooldown and a price means the pool is
            // the only thing holding the ability back, which is a different
            // kind of ability from one on a timer — a Caster with Mana in hand
            // can cast twice in a row and a Tank waiting on Grit cannot. No
            // cooldown and NO price means nothing gates it at all, which is
            // worth saying out loud rather than leaving as a blank space.
            Line.GateText = ResourceCost > 0.0f
                ? FString(TEXT("COST GATED")) : FString(TEXT("NO COST"));
        }
        if (CastTimeSeconds > 0.0f)
        {
            Line.CastText = FormatSeconds(CastTimeSeconds) + TEXT(" CAST");
        }
        return Line;
    }

    // The same three runs as one string, for the places that have room for a
    // line and not for a row — the slot chips at the top of the screen.
    inline FString Flatten(const FCostLine& Line)
    {
        TArray<FString> Parts;
        if (!Line.CostText.IsEmpty()) Parts.Add(Line.CostText);
        if (!Line.GateText.IsEmpty()) Parts.Add(Line.GateText);
        if (!Line.CastText.IsEmpty()) Parts.Add(Line.CastText);
        return FString::Join(Parts, TEXT("  "));
    }
}
