#pragma once
#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// O252: THE ABILITY SKILL LEVEL, 1-15, ON A SHARED POOL.
//
// Every ability a character owns sits at the SAME skill level. That is the
// whole point of the shared pool rather than per-ability experience: with six
// abilities to a class and token-gated unlocks, per-ability XP would make
// every new unlock a downgrade against the one you have been using, and a
// player who will not try the new thing is the opposite of the build variety
// the roster exists for.
//
// DERIVED FROM CHARACTER LEVEL RATHER THAN ACCUMULATED SEPARATELY. A shared
// pool fed by the same combat that feeds character XP is a different-shaped
// function of the same input, so it is written as that function instead of as
// a second accumulator with a second save field and a second migration. The
// observable properties O252 asks for all hold: the range is 1-15, every
// ability shares one level, and nothing is lost by swapping. If pacing ever
// needs to diverge from character level — skill level from combat only, say,
// where character XP also comes from quests — ForCharacterLevel is the one
// function to replace and nothing above it changes.
//
// The multiplier scales an ability's AUTHORED base only. Gear's Added Ability
// Power is added after, so a skill level raises what the ability itself does
// and never multiplies what the player bolted onto it — the same partition
// the weapon lane keeps between a gun's base damage and Added Damage.
// ---------------------------------------------------------------------------
namespace BreakerSkillLevel
{
    constexpr int32 MinLevel = 1;
    constexpr int32 MaxLevel = 15;          // O252

    // What one level is worth, as a fraction of the ability's authored base.
    // At MaxLevel this composes to x1.84 over the life of a character.
    constexpr float DamagePerLevel = 0.06f; // O2 PLACEHOLDER

    /**
     * The skill level a character of this level carries. Integer maths on
     * purpose: the ladder has fifteen rungs and a player standing on one of
     * them should be able to read which, so this never returns a fraction and
     * never rounds differently on two machines.
     *
     * MinLevel at character level 1 and MaxLevel at the character cap, by
     * construction rather than by a table that could disagree with itself.
     */
    inline int32 ForCharacterLevel(int32 CharacterLevel, int32 CharacterMaxLevel)
    {
        if (CharacterMaxLevel <= 1) return MinLevel;
        const int32 Clamped = FMath::Clamp(CharacterLevel, 1, CharacterMaxLevel);
        const int32 Span = MaxLevel - MinLevel;
        const int32 Steps = ((Clamped - 1) * Span) / (CharacterMaxLevel - 1);
        return FMath::Clamp(MinLevel + Steps, MinLevel, MaxLevel);
    }

    /** What an ability's authored base is multiplied by at this skill level. */
    inline float DamageMultiplier(int32 SkillLevel)
    {
        const int32 Level = FMath::Clamp(SkillLevel, MinLevel, MaxLevel);
        return 1.0f + DamagePerLevel * static_cast<float>(Level - MinLevel);
    }
}
