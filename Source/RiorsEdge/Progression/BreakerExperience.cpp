#include "Progression/BreakerExperience.h"

int32 UBreakerExperienceLibrary::XpToNextLevel(int32 Level, const FBreakerExperienceCurve& Curve)
{
    // The hard stop falls out here rather than being special-cased at every
    // call site: at or past the cap the next level costs nothing because there
    // is no next level. LevelForTotalXp's loop terminates on exactly this.
    if (Level < 1 || Level >= MaxCharacterLevel) return 0;
    const float Cost = static_cast<float>(Curve.BaseXpPerLevel) * FMath::Pow(static_cast<float>(Level), Curve.LevelExponent);
    return FMath::Max(1, FMath::RoundToInt(Cost));
}

int32 UBreakerExperienceLibrary::TotalXpToReachLevel(int32 Level, const FBreakerExperienceCurve& Curve)
{
    const int32 Target = FMath::Clamp(Level, 1, MaxCharacterLevel);
    int32 Total = 0;
    for (int32 Step = 1; Step < Target; ++Step)
    {
        Total += XpToNextLevel(Step, Curve);
    }
    return Total;
}

int32 UBreakerExperienceLibrary::LevelForTotalXp(int32 TotalXp, const FBreakerExperienceCurve& Curve)
{
    // Walks the ladder rather than inverting the power curve in closed form.
    // Fifty steps of integer addition is nothing, and it is EXACTLY consistent
    // with TotalXpToReachLevel by construction — a closed-form inverse would
    // disagree with the summed forward curve at the rounding boundaries, which
    // is precisely the class of drift that put a character one XP short of a
    // level it had already been shown.
    if (TotalXp <= 0) return 1;
    int32 Level = 1;
    int32 Spent = 0;
    while (Level < MaxCharacterLevel)
    {
        const int32 Step = XpToNextLevel(Level, Curve);
        if (Step <= 0 || Spent + Step > TotalXp) break;
        Spent += Step;
        ++Level;
    }
    return Level;
}

float UBreakerExperienceLibrary::LevelProgressFraction(int32 TotalXp, const FBreakerExperienceCurve& Curve)
{
    const int32 Level = LevelForTotalXp(TotalXp, Curve);
    const int32 Step = XpToNextLevel(Level, Curve);
    // At the cap the bar reads FULL, not empty. An empty bar at level 50 says
    // "you have made no progress" about the one state where no progress is
    // possible, which is the opposite of what the player has achieved.
    if (Step <= 0) return 1.0f;
    const int32 Into = TotalXp - TotalXpToReachLevel(Level, Curve);
    return FMath::Clamp(static_cast<float>(Into) / static_cast<float>(Step), 0.0f, 1.0f);
}

int32 UBreakerExperienceLibrary::XpForKill(EBreakerMonsterRank Rank, int32 AreaLevel, const FBreakerExperienceCurve& Curve)
{
    float Multiplier = 1.0f;
    switch (Rank)
    {
    case EBreakerMonsterRank::Elite:           Multiplier = Curve.EliteXpMultiplier; break;
    case EBreakerMonsterRank::ModifierBearing: Multiplier = Curve.ModifierBearingXpMultiplier; break;
    case EBreakerMonsterRank::Boss:            Multiplier = Curve.BossXpMultiplier; break;
    default:                                   Multiplier = 1.0f; break;
    }
    // Area level, not character level: the reward tracks the CONTENT. Rewarding
    // by character level would pay a level-50 player the same for farming area
    // level 1 as for fighting on-level, which is the shape that makes the
    // easiest content the fastest content.
    const float LevelScalar = 1.0f + FMath::Max(0, AreaLevel - 1) * Curve.XpPerAreaLevel;
    return FMath::Max(0, FMath::RoundToInt(static_cast<float>(Curve.BaseKillXp) * Multiplier * LevelScalar));
}

FBreakerLevellingProjection UBreakerExperienceLibrary::ProjectLevellingRate(
    const FBreakerKillRateSample& Kills, int32 AreaLevel, const FBreakerExperienceCurve& Curve)
{
    FBreakerLevellingProjection Projection;

    const EBreakerMonsterRank Ranks[] = {
        EBreakerMonsterRank::Trash, EBreakerMonsterRank::Elite,
        EBreakerMonsterRank::ModifierBearing, EBreakerMonsterRank::Boss };
    const float PerHour[] = {
        Kills.TrashKillsPerHour, Kills.EliteKillsPerHour,
        Kills.ModifierBearingKillsPerHour, Kills.BossKillsPerHour };

    for (int32 Index = 0; Index < 4; ++Index)
    {
        Projection.XpPerHour += PerHour[Index] * static_cast<float>(XpForKill(Ranks[Index], AreaLevel, Curve));
    }

    Projection.TotalXpToCap = TotalXpToReachLevel(MaxCharacterLevel, Curve);
    Projection.HoursToCap = Projection.XpPerHour > 0.0f
        ? static_cast<float>(Projection.TotalXpToCap) / Projection.XpPerHour : 0.0f;

    // Trash is the kill a player actually strings together, so "how many
    // ordinary kills is my first level" is the honest perceptibility figure —
    // an average across ranks would be flattered by the boss.
    const int32 TrashXp = XpForKill(EBreakerMonsterRank::Trash, AreaLevel, Curve);
    Projection.KillsForFirstLevel = TrashXp > 0
        ? FMath::CeilToInt(static_cast<float>(XpToNextLevel(1, Curve)) / static_cast<float>(TrashXp)) : 0;

    return Projection;
}
