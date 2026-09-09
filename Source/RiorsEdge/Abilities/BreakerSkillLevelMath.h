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
    constexpr int32 MaxLevel = 15;          // O252 — the EARNED ceiling

    // ---- O253: the levels gear grants, above the earned ceiling ------------
    // GEAR OVERCAPS, and it has to. O253 puts +3 at T3-T1 and +4 at T-1, and
    // says the top of that ladder is endgame by construction — but the endgame
    // is exactly where every character is level 50 and therefore already at
    // MaxLevel. Clamping the composed level to 15 would make three quarters of
    // the affix's own ladder grant literally nothing, which is this project's
    // "content the player cannot reach is not built" rule failing at the top
    // end of the design it is meant to serve. So the EARNED range stays 1-15
    // and gear adds above it.
    constexpr int32 MaxAffixLevels = 4;     // O253, per affix

    // Both slots O253 names can carry the line, so the composed ceiling is the
    // earned ceiling plus two full rolls. WHETHER THE TWO SLOTS SHOULD STACK
    // IS AN OPEN QUESTION for the owner (see .claude/DESK.md): they are summed
    // here because O253 authors a per-affix ladder and says nothing about a
    // total, and because capping the sum makes a second good roll worth
    // nothing, which is its own bad feeling. Capping it later is one constant.
    constexpr int32 AffixSlotCount = 2;     // Waist and Necklace (O253)
    constexpr int32 MaxComposedLevel = MaxLevel + MaxAffixLevels * AffixSlotCount;

    // What one level is worth, as a fraction of the ability's authored base.
    // At MaxLevel this composes to x1.84 over the life of a character.
    constexpr float DamagePerLevel = 0.06f; // O2 PLACEHOLDER

    /**
     * How many skill levels one rolled affix grants at this tier. O253's bands
     * exactly: +1 at T12-T7, +2 at T6-T4, +3 at T3-T1, +4 at T-1 alone.
     *
     * A TABLE RATHER THAN A CURVE, and that is forced rather than chosen: the
     * ordinary magnitude path interpolates geometrically between two authored
     * anchors, and no pair of anchors reproduces these bands under either
     * rounding rule — the required ranges are empty. A skill level is also a
     * whole number by nature; +2.37 levels is not a thing the player can be
     * shown. The step table IS the ruling, so it is written once, here, where
     * the rest of the skill-level arithmetic lives.
     *
     * T0 is not named by O253 and falls in the +3 band with T1, which is the
     * same treatment Pierce gives it — the step is reserved for T-1.
     */
    inline int32 AffixLevelsForTier(int32 Tier)
    {
        if (Tier <= -1) return 4;
        if (Tier <= 3)  return 3;
        if (Tier <= 6)  return 2;
        return 1;
    }

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

    /**
     * What an ability's authored base is multiplied by at this skill level.
     * Clamped to MaxComposedLevel, not MaxLevel: gear legitimately carries a
     * character past the earned ceiling, and clamping at 15 here is precisely
     * the bug that would make O253's endgame tiers pay nothing.
     */
    inline float DamageMultiplier(int32 SkillLevel)
    {
        const int32 Level = FMath::Clamp(SkillLevel, MinLevel, MaxComposedLevel);
        return 1.0f + DamagePerLevel * static_cast<float>(Level - MinLevel);
    }
}
