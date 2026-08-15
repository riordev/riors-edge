#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Combat/BreakerMonsterChassis.h"
#include "Items/BreakerDropTable.h"
#include "BreakerExperience.generated.h"

// ---------------------------------------------------------------------------
// THE XP LOOP
// ---------------------------------------------------------------------------
// Until this existed, `FBreakerProgressionState::CharacterLevel` was declared
// with a default of 1 and NOTHING in the project ever wrote it — which is what
// made Swift's third jump unreachable by construction and what prompted
// ruling O40(b): "until an XP loop exists, no system may gate on
// CharacterLevel". This is that loop, so O40(b)'s condition is now met and
// gates may be re-opened BY RULING — not automatically, and not by this file.
//
// Locked decisions this obeys, none of them re-litigated here:
//  - The level cap is 50, a HARD stop. There is no post-cap character power;
//    all endgame power comes from gear (O29). LevelForTotalXp therefore clamps
//    and XpToNextLevel returns 0 at the cap rather than growing forever.
//  - Solo is the balance target.
//
// Every value below is O2 PLACEHOLDER. O2's second half matters as much as its
// first: a placeholder must be PERCEPTIBLE in the gym, so these are seeded so
// that ordinary fighting visibly moves the bar within a minute rather than
// being technically correct and invisible. `ProjectLevellingRate` exists so the
// pace is derivable arithmetic rather than a claim, mirroring the drop table's
// `ProjectLootRate` — and, like it, is pinned against a simulation in test.
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerExperienceCurve
{
    GENERATED_BODY()

    // XP required to go from level 1 to level 2. Every later step scales off
    // this, so it is the single knob that moves the whole ladder's pace.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 BaseXpPerLevel = 240;   // O2 PLACEHOLDER

    // Curve shape. 1.0 is linear (level 40 costs 40x level 1); above 1.0
    // back-loads the ladder so late levels are events. Deliberately gentler
    // than the affix ladder's 1.25 (O29): gear depth is where the endgame
    // lives, so the LEVELLING curve should not also try to be the long tail.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1.0")) float LevelExponent = 1.45f;   // O2 PLACEHOLDER

    // Base XP a single kill pays, before rank and level scaling.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 BaseKillXp = 12;   // O2 PLACEHOLDER

    // Rank multipliers. Mirrors the drop table's rank ladder in SHAPE so a
    // player's two reward channels agree about what a boss is worth relative
    // to a trash mob, rather than each inventing its own hierarchy.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float EliteXpMultiplier = 4.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float ModifierBearingXpMultiplier = 7.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float BossXpMultiplier = 40.0f;   // O2 PLACEHOLDER

    // How much an on-level kill pays relative to a level-1 one. The area-level
    // ladder runs to 100 while the character cap is 50, so this is what stops
    // a level-50 player farming area level 1 forever: the reward tracks the
    // content, not the character.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float XpPerAreaLevel = 0.08f;   // O2 PLACEHOLDER
};

// What a levelling pace looks like as arithmetic rather than as a claim.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerLevellingProjection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float XpPerHour = 0.0f;
    UPROPERTY(BlueprintReadOnly) int32 TotalXpToCap = 0;
    UPROPERTY(BlueprintReadOnly) float HoursToCap = 0.0f;
    // Kills at the sampled rate to clear the FIRST level — the number that
    // decides whether the loop reads as alive in the first minute of a
    // playtest, which is O2's perceptibility requirement.
    UPROPERTY(BlueprintReadOnly) int32 KillsForFirstLevel = 0;
};

UCLASS()
class RIORSEDGE_API UBreakerExperienceLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // The hard cap. A locked decision, not a tunable: no post-cap character
    // power exists, so this is not a number anyone may raise casually.
    static constexpr int32 MaxCharacterLevel = 50;

    // XP required to advance FROM Level to Level+1. Zero at or past the cap,
    // which is what makes the hard stop fall out of the arithmetic rather than
    // needing a special case at every call site.
    UFUNCTION(BlueprintPure, Category="Progression|XP")
    static int32 XpToNextLevel(int32 Level, const FBreakerExperienceCurve& Curve);

    // Cumulative XP required to REACH Level from level 1. Reaching level 1
    // costs nothing.
    UFUNCTION(BlueprintPure, Category="Progression|XP")
    static int32 TotalXpToReachLevel(int32 Level, const FBreakerExperienceCurve& Curve);

    // The inverse: what level a total XP figure buys, clamped to the cap.
    // Total XP is the stored quantity rather than level-plus-remainder,
    // because it is the representation that cannot drift out of agreement
    // with the curve when the curve is retuned — a saved character re-derives
    // its level from the new curve instead of keeping a level it can no longer
    // afford.
    UFUNCTION(BlueprintPure, Category="Progression|XP")
    static int32 LevelForTotalXp(int32 TotalXp, const FBreakerExperienceCurve& Curve);

    // Progress through the CURRENT level, 0..1. Returns 1 at the cap so a HUD
    // bar reads full rather than empty at the end of the ladder.
    UFUNCTION(BlueprintPure, Category="Progression|XP")
    static float LevelProgressFraction(int32 TotalXp, const FBreakerExperienceCurve& Curve);

    // What one kill pays.
    UFUNCTION(BlueprintPure, Category="Progression|XP")
    static int32 XpForKill(EBreakerMonsterRank Rank, int32 AreaLevel, const FBreakerExperienceCurve& Curve);

    // Analytic pace, mirroring UBreakerDropTableLibrary::ProjectLootRate: the
    // documented figure and the shipped one are pinned to each other by test,
    // so the pace cannot drift from what this reports.
    UFUNCTION(BlueprintPure, Category="Progression|XP")
    static FBreakerLevellingProjection ProjectLevellingRate(
        const FBreakerKillRateSample& Kills, int32 AreaLevel, const FBreakerExperienceCurve& Curve);
};
