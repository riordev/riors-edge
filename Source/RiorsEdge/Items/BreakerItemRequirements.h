#pragma once

#include "CoreMinimal.h"
#include "Progression/BreakerExperience.h"

// ---------------------------------------------------------------------------
// The equip requirement, pure (Part One-AA). DERIVED, NEVER STORED: a stored
// required level would freeze today's curve into every save ever written and
// the first retune would need a migration — the exact mistake
// BreakerItemBaseStats.h's own comment exists to prevent. No field on
// FBreakerItemInstance, no SaveVersion bump, and a retune is a one-line edit.
//
// THE GATE EXISTS ONLY WHILE LEVELLING DOES: RequiredLevel is clamped at the
// character cap because three ceilings share one ladder (character 50, area
// 100, item 120) and an item level 100 cannot require a level nothing
// reaches. Past 50 the gate is a no-op — correct, not a compromise: the
// owner's stated purpose is gauging difficulty WHILE LEVELLING, and a rule
// that expires when its purpose does has no blast radius (GetDropItemLevel's
// identity and the TTK cancellation are untouched).
//
// WHERE IT IS CALLED, and the loophole this list guards: EquipItem stays the
// ungated MECHANISM (32 call sites, most on bare rigs); every PLAYER-FACING
// entry point consults the predicate instead — EquipFromBackpack today, the
// stash withdrawal when One-X lands. A new player-facing entry point that
// does not call this is the gate's only bypass; the requirement tests pin
// the roster.
// ---------------------------------------------------------------------------
namespace BreakerItemRequirements
{
    // O263: the opening window. Item levels at or under this need no level at
    // all. Owner report: twenty minutes of play produced items the character
    // could not wear, because an ilvl 7 drop wanted character level 7 — the
    // gate exists to PACE levelling and has nothing to pace in the first hour,
    // where it only turns the game's first drops into inventory clutter.
    constexpr int32 FreeEquipItemLevel = 10;   // O2 PLACEHOLDER (O263)

    inline int32 RequiredLevelFor(int32 ItemLevel)
    {
        // RULED (One-AA): min(ItemLevel, MaxCharacterLevel), floored at 1 so
        // a degenerate item level cannot author a level-0 requirement.
        // O263 moves that floor up to cover the whole opening window; the rule
        // stays derived, so this remains a one-line retune with no migration.
        if (ItemLevel <= FreeEquipItemLevel) return 1;
        return FMath::Clamp(ItemLevel, 1, UBreakerExperienceLibrary::MaxCharacterLevel);
    }

    inline bool CanEquipAtLevel(int32 ItemLevel, int32 CharacterLevel)
    {
        return CharacterLevel >= RequiredLevelFor(ItemLevel);
    }
}
